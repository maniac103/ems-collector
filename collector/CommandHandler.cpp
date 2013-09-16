/*
 * Buderus EMS data collector
 *
 * Copyright (C) 2011 Danny Baumann <dannybaumann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/byteorder.h>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "CommandHandler.h"

CommandHandler::CommandHandler(TcpHandler& handler,
			       boost::asio::ip::tcp::endpoint& endpoint) :
    m_handler(handler),
    m_acceptor(handler, endpoint)
{
    startAccepting();
}

CommandHandler::~CommandHandler()
{
    m_acceptor.close();
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&CommandConnection::close, _1));
    m_connections.clear();
}

void
CommandHandler::handleAccept(CommandConnection::Ptr connection,
			     const boost::system::error_code& error)
{
    if (error) {
	if (error != boost::asio::error::operation_aborted) {
	    std::cerr << "Accept error: " << error.message() << std::endl;
	}
	return;
    }

    startConnection(connection);
    startAccepting();
}

void
CommandHandler::startConnection(CommandConnection::Ptr connection)
{
    m_connections.insert(connection);
    connection->startRead();
}

void
CommandHandler::stopConnection(CommandConnection::Ptr connection)
{
    m_connections.erase(connection);
    connection->close();
}

void
CommandHandler::handlePcMessage(const EmsMessage& message)
{
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&CommandConnection::handlePcMessage,
			      _1, message));
}

void
CommandHandler::startAccepting()
{
    CommandConnection::Ptr connection(new CommandConnection(*this));
    m_acceptor.async_accept(connection->socket(),
		            boost::bind(&CommandHandler::handleAccept, this,
					connection, boost::asio::placeholders::error));
}


CommandConnection::CommandConnection(CommandHandler& handler) :
    m_socket(handler.getHandler()),
    m_handler(handler),
    m_waitingForResponse(false),
    m_nextCommandTimer(handler.getHandler()),
    m_responseTimeout(handler.getHandler()),
    m_responseCounter(0)
{
}

void
CommandConnection::handleRequest(const boost::system::error_code& error)
{
    if (error) {
	if (error != boost::asio::error::operation_aborted) {
	    m_handler.stopConnection(shared_from_this());
	}
	return;
    }

    std::istream requestStream(&m_request);

    if (m_waitingForResponse) {
	respond("ERRBUSY");
    } else if (m_request.size() > 2) {
	CommandResult result = handleCommand(requestStream);

	switch (result) {
	    case Ok:
		break;
	    case InvalidCmd:
		respond("ERRCMD");
		break;
	    case InvalidArgs:
		respond("ERRARGS");
		break;
	}
    }

    /* drain remainder */
    std::string remainder;
    std::getline(requestStream, remainder);

    startRead();
}

void
CommandConnection::handleWrite(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	m_handler.stopConnection(shared_from_this());
    }
}

CommandConnection::CommandResult
CommandConnection::handleCommand(std::istream& request)
{
    std::string category;
    request >> category;

    if (category == "hk1") {
	return handleHkCommand(request, 61);
    } else if (category == "hk2") {
	return handleHkCommand(request, 71);
    } else if (category == "hk3") {
	return handleHkCommand(request, 81);
    } else if (category == "hk4") {
	return handleHkCommand(request, 91);
    } else if (category == "ww") {
	return handleWwCommand(request);
    } else if (category == "geterrors") {
	handleGetErrorsCommand(0);
	return Ok;
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleHkCommand(std::istream& request, uint8_t type)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	uint8_t data[2];
	std::string mode;

	request >> mode;

	data[0] = 0x07;
	if (mode == "day")        data[1] = 0x01;
	else if (mode == "night") data[1] = 0x00;
	else if (mode == "auto")  data[1] = 0x02;
	else return InvalidArgs;

	sendCommand(EmsMessage::addressRC, type, data, sizeof(data));
	return Ok;
    } else if (cmd == "daytemperature") {
	return handleHkTemperatureCommand(request, type, 2);
    } else if (cmd == "nighttemperature") {
	return handleHkTemperatureCommand(request, type, 1);
    } else if (cmd == "holidaytemperature") {
	return handleHkTemperatureCommand(request, type, 3);
    } else if (cmd == "holidaymode") {
	return handleSetHolidayCommand(request, type + 2, 93);
    } else if (cmd == "vacationmode") {
	return handleSetHolidayCommand(request, type + 2, 87);
    } else if (cmd == "partymode") {
	uint8_t data[2];
	unsigned int hours;

	request >> hours;

	if (!request || hours > 99) {
	    return InvalidArgs;
	}
	data[0] = 86;
	data[1] = hours;

	sendCommand(EmsMessage::addressRC, type, data, sizeof(data));
	return Ok;
    } else if (cmd == "schedule") {
	unsigned int index;
	uint8_t data[1 + sizeof(EmsMessage::ScheduleEntry)];
	EmsMessage::ScheduleEntry *entry = (EmsMessage::ScheduleEntry *) &data[1];

	request >> index;

	if (!request || index > 42 || !parseScheduleEntry(request, entry)) {
	    return InvalidArgs;
	}

	data[0] = (index - 1) * sizeof(EmsMessage::ScheduleEntry);

	sendCommand(EmsMessage::addressRC, type + 2, data, sizeof(data));
	return Ok;
    } else if (cmd == "getschedule") {
	handleGetScheduleCommand(type + 2, 0);
	return Ok;
    } else if (cmd == "getholiday") {
	const size_t msgSize = sizeof(EmsMessage::HolidayEntry);
	uint8_t data[] = { 87, 2 * msgSize };

	sendCommand(EmsMessage::addressRC, type + 2, data, sizeof(data), true);
	return Ok;
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleHkTemperatureCommand(std::istream& request, uint8_t type, uint8_t offset)
{
    uint8_t data[2];
    float value;
    uint8_t valueByte;

    request >> value;
    if (!request) {
	return InvalidArgs;
    }

    try {
	valueByte = boost::numeric_cast<uint8_t>(2 * value);
	if (valueByte < 20 || valueByte > 60) {
	    return InvalidArgs;
	}
    } catch (boost::numeric::bad_numeric_cast& e) {
	return InvalidArgs;
    }

    data[0] = offset;
    data[1] = valueByte;
    sendCommand(EmsMessage::addressRC, type, data, sizeof(data));
    return Ok;
}

CommandConnection::CommandResult
CommandConnection::handleSetHolidayCommand(std::istream& request, uint8_t type, uint8_t offset)
{
    uint8_t data[1 + 2 * sizeof(EmsMessage::HolidayEntry)];
    std::string beginString, endString;
    EmsMessage::HolidayEntry *begin = (EmsMessage::HolidayEntry *) &data[1];
    EmsMessage::HolidayEntry *end = (EmsMessage::HolidayEntry *) &data[1 + sizeof(*begin)];

    request >> beginString;
    request >> endString;

    if (!request) {
	return InvalidArgs;
    }

    data[0] = offset;
    if (!parseHolidayEntry(beginString, begin) || !parseHolidayEntry(endString, end)) {
	return InvalidArgs;
    }

    /* make sure begin is not later than end */
    if (begin->year > end->year) {
	return InvalidArgs;
    } else if (begin->year == end->year) {
	if (begin->month > end->month) {
	    return InvalidArgs;
	} else if (begin->month == end->month) {
	    if (begin->day > end->day) {
		return InvalidArgs;
	    }
	}
    }

    sendCommand(EmsMessage::addressRC, type, data, sizeof(data));
    return Ok;
}

CommandConnection::CommandResult
CommandConnection::handleWwCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "thermdesinfect") {
	return handleThermDesinfectCommand(request);
    } else if (cmd == "zirkpump") {
	return handleZirkPumpCommand(request);
    } else if (cmd == "mode") {
	uint8_t data[2];
	std::string mode;

	request >> mode;

	data[0] = 0x02;
	if (mode == "on")        data[1] = 0x01;
	else if (mode == "off")  data[1] = 0x00;
	else if (mode == "auto") data[1] = 0x02;
	else return InvalidArgs;

	sendCommand(EmsMessage::addressRC, 0x37, data, sizeof(data));
	return Ok;
    } else if (cmd == "temperature") {
	uint8_t data[2];
	unsigned int temperature;

	request >> temperature;

	if (!request || temperature < 30 || temperature > 60) {
	    return InvalidArgs;
	}

	data[0] = 0x02;
	data[1] = temperature;
	sendCommand(EmsMessage::addressUBA, 0x33, data, sizeof(data));
	return Ok;
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleThermDesinfectCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	uint8_t data[2];
	std::string mode;

	request >> mode;

	data[0] = 0x04;
	if (mode == "on")       data[1] = 0xff;
	else if (mode == "off") data[1] = 0x00;
	else return InvalidArgs;

	sendCommand(EmsMessage::addressRC, 0x37, data, sizeof(data));
	return Ok;
    } else if (cmd == "day") {
	uint8_t data[2];
	std::string day;

	request >> day;

	data[0] = 0x05;
	if (day == "monday")         data[1] = 0x00;
	else if (day == "tuesday")   data[1] = 0x01;
	else if (day == "wednesday") data[1] = 0x02;
	else if (day == "thursday")  data[1] = 0x03;
	else if (day == "friday")    data[1] = 0x04;
	else if (day == "saturday")  data[1] = 0x05;
	else if (day == "sunday")    data[1] = 0x06;
	else if (day == "everyday")  data[1] = 0x07;
	else return InvalidArgs;

	sendCommand(EmsMessage::addressRC, 0x37, data, sizeof(data));
	return Ok;
    } else if (cmd == "temperature") {
	uint8_t data[2];
	unsigned int temperature;

	request >> temperature;

	if (!request || temperature < 60 || temperature > 80) {
	    return InvalidArgs;
	}

	data[0] = 0x08;
	data[1] = temperature;
	sendCommand(EmsMessage::addressUBA, 0x33, data, sizeof(data));
	return Ok;
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleZirkPumpCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	uint8_t data[2];
	std::string mode;

	request >> mode;

	data[0] = 0x03;
	if (mode == "on")        data[1] = 0x01;
	else if (mode == "off")  data[1] = 0x00;
	else if (mode == "auto") data[1] = 0x02;
	else return InvalidArgs;

	sendCommand(EmsMessage::addressRC, 0x37, data, sizeof(data));
	return Ok;
    } else if (cmd == "count") {
	uint8_t data[2];
	std::string countString;

	request >> countString;

	data[0] = 0x07;
	if (countString == "alwayson") {
	    data[1] = 0x07;
	} else {
	    try {
		unsigned int count = boost::lexical_cast<unsigned int>(countString);
		if (count < 1 || count > 6) {
		    return InvalidArgs;
		}
		data[1] = count;
	    } catch (boost::bad_lexical_cast& e) {
		return InvalidArgs;
	    }
	}
	sendCommand(EmsMessage::addressUBA, 0x33, data, sizeof(data));
	return Ok;
    }

    return InvalidCmd;
}

void
CommandConnection::handleGetErrorsCommand(unsigned int offset)
{
    /* Service Key: 0x04 0x10 0x12 = 0x04 0xXX 0xYY
     * -> 0xXX | 0x80, 0xYY, 0x00, <maxlen>
     */
    const size_t msgSize = sizeof(EmsMessage::ErrorRecord);
    uint8_t offsetBytes = (uint8_t) (offset * msgSize);
    uint8_t data[] = { offsetBytes, 2 * msgSize };

    m_responseCounter = offset;
    sendCommand(EmsMessage::addressRC, 0x12, data, sizeof(data), true);
}

void
CommandConnection::handleGetScheduleCommand(uint8_t type, unsigned int offset)
{
    const size_t msgSize = sizeof(EmsMessage::ScheduleEntry);
    uint8_t offsetBytes = (uint8_t) (offset * msgSize);
    uint8_t data[] = { offsetBytes, 14 * msgSize };

    m_responseCounter = offset;

    sendCommand(EmsMessage::addressRC, type, data, sizeof(data), true);
}

void
CommandConnection::handlePcMessage(const EmsMessage& message)
{
    if (!m_waitingForResponse) {
	return;
    }

    const std::vector<uint8_t>& data = message.getData();

    if (message.getDestination() == EmsMessage::addressPC && message.getType() == 0xff) {
	m_waitingForResponse = false;
	respond(data[0] == 0x04 ? "FAIL" : "OK");
    }
    if (message.getSource() == EmsMessage::addressRC) {
	/* strip offset */
	size_t offset = 1;
	bool done = false;

	m_responseTimeout.cancel();

	switch (message.getType()) {
	    case 0x12: /* get errors */ {
		const size_t msgSize = sizeof(EmsMessage::ErrorRecord);
		while (offset + msgSize <= data.size()) {
		    EmsMessage::ErrorRecord *record = (EmsMessage::ErrorRecord *) &data.at(offset);
		    std::string response = buildErrorMessageResponse(record);

		    if (!response.empty()) {
			respond(response);
		    }
		    m_responseCounter++;
		    offset += msgSize;
		}

		if (m_responseCounter < 4) {
		    m_nextCommandTimer.expires_from_now(boost::posix_time::milliseconds(500));
		    m_nextCommandTimer.async_wait(boost::bind(&CommandConnection::handleGetErrorsCommand,
						  this, m_responseCounter));
		} else {
		    done = true;
		}
		break;
	    }
	    case 0x3f: /* get schedule HK1 */
	    case 0x49: /* get schedule HK2 */
	    case 0x53: /* get schedule HK3 */
	    case 0x5d: /* get schedule HK4 */
		if (data[0] > 80) {
		    /* it's at the end -> holiday schedule */
		    const size_t msgSize = sizeof(EmsMessage::HolidayEntry);

		    if (data.size() > 2 * msgSize) {
			EmsMessage::HolidayEntry *begin = (EmsMessage::HolidayEntry *) &data.at(offset);
			EmsMessage::HolidayEntry *end = (EmsMessage::HolidayEntry *) &data.at(offset + msgSize);
			respond(buildHolidayEntryResponse("BEGIN", begin));
			respond(buildHolidayEntryResponse("END", end));
			done = true;
		    } else {
			respond("FAIL");
		    }
		} else {
		    /* it's at the beginning -> heating schedule */
		    const size_t msgSize = sizeof(EmsMessage::ScheduleEntry);

		    while (offset + msgSize <= data.size()) {
			EmsMessage::ScheduleEntry *entry = (EmsMessage::ScheduleEntry *) &data.at(offset);
			std::string response = buildScheduleEntryResponse(entry);

			if (!response.empty()) {
			    respond(response);
			} else {
			    done = true;
			}
			m_responseCounter++;
			offset += msgSize;
		    }

		    if (m_responseCounter < 42 && !done) {
			m_nextCommandTimer.expires_from_now(boost::posix_time::milliseconds(500));
			m_nextCommandTimer.async_wait(boost::bind(&CommandConnection::handleGetScheduleCommand,
						      this, message.getType(), m_responseCounter));
		    } else {
			done = true;
		    }
		}
		break;
	}

	if (done) {
	    m_waitingForResponse = false;
	    respond("OK");
	}
    }
}

void
CommandConnection::scheduleResponseTimeout()
{
    m_waitingForResponse = true;
    m_responseTimeout.expires_from_now(boost::posix_time::seconds(1));
    m_responseTimeout.async_wait(boost::bind(&CommandConnection::responseTimeout,
					     this, boost::asio::placeholders::error));
}

void
CommandConnection::responseTimeout(const boost::system::error_code& error)
{
    if (m_waitingForResponse && error != boost::asio::error::operation_aborted) {
	respond("ERRTIMEOUT");
	m_waitingForResponse = false;
    }
}

std::string
CommandConnection::buildErrorMessageResponse(const EmsMessage::ErrorRecord *record)
{
    if (record->errorAscii[0] == 0) {
	/* no error at this position */
	return "";
    }

    std::ostringstream response;

    if (record->hasDate) {
	response << std::setw(2) << std::setfill('0') << (unsigned int) record->day << "-";
	response << std::setw(2) << std::setfill('0') << (unsigned int) record->month << "-";
	response << std::setw(4) << (unsigned int) (2000 + record->year) << " ";
	response << std::setw(2) << std::setfill('0') << (unsigned int) record->hour << "-";
	response << std::setw(2) << std::setfill('0') << (unsigned int) record->minute;
    } else {
	response  << "---";
    }

    response << " ";
    response << std::hex << (unsigned int) record->source << " ";

    response << std::dec << record->errorAscii[0] << record->errorAscii[1] << " ";
    response << __be16_to_cpu(record->code_be16) << " ";
    response << __be16_to_cpu(record->durationMinutes_be16);

    return response.str();
}

static const char * dayNames[] = {
    "MO", "TU", "WE", "TH", "FR", "SA", "SU"
};

std::string
CommandConnection::buildScheduleEntryResponse(const EmsMessage::ScheduleEntry *entry)
{
    if (entry->time >= 0x90) {
	/* unset */
	return "";
    }

    std::ostringstream response;
    unsigned int minutes = entry->time * 10;
    response << dayNames[entry->day / 2] << " ";
    response << std::setw(2) << std::setfill('0') << (minutes / 60) << ":";
    response << std::setw(2) << std::setfill('0') << (minutes % 60) << " ";
    response << (entry->on ? "ON" : "OFF");

    return response.str();
}

bool
CommandConnection::parseScheduleEntry(std::istream& request, EmsMessage::ScheduleEntry *entry)
{
    std::string day, time, mode;

    request >> day;
    if (!request) {
	return false;
    }

    if (day == "unset") {
	entry->on = 7;
	entry->day = 0xe;
	entry->time = 0x90;
	return true;
    }

    request >> time >> mode;
    if (!request) {
	return false;
    }

    if (mode == "ON") {
	entry->on = 1;
    } else if (mode == "OFF") {
	entry->on = 0;
    } else {
	return false;
    }

    bool hasDay = false;
    for (size_t i = 0; i < sizeof(dayNames) / sizeof(dayNames[0]); i++) {
	if (day == dayNames[i]) {
	    entry->day = 2 * i;
	    hasDay = true;
	    break;
	}
    }
    if (!hasDay) {
	return false;
    }

    size_t pos = time.find(":");
    if (pos == std::string::npos) {
	return false;
    }
    unsigned int hours = boost::lexical_cast<unsigned int>(time.substr(0, pos));
    unsigned int minutes = boost::lexical_cast<unsigned int>(time.substr(pos + 1));
    if (hours > 23 || minutes >= 60 || (minutes % 10) != 0) {
	return false;
    }

    entry->time = (uint8_t) ((hours * 60 + minutes) / 10);

    return true;
}

std::string
CommandConnection::buildHolidayEntryResponse(const char *type, const EmsMessage::HolidayEntry *entry)
{
    std::ostringstream response;

    response << type << ";";
    response << std::setw(2) << std::setfill('0') << (unsigned int) entry->day << "-";
    response << std::setw(2) << std::setfill('0') << (unsigned int) entry->month << "-";
    response << std::setw(4) << (unsigned int) (2000 + entry->year);

    return response.str();
}

bool
CommandConnection::parseHolidayEntry(const std::string& string, EmsMessage::HolidayEntry *entry)
{
    size_t pos = string.find('-');
    if (pos == std::string::npos) {
	return false;
    }

    size_t pos2 = string.find('-', pos + 1);
    if (pos2 == std::string::npos) {
	return false;
    }

    unsigned int day = boost::lexical_cast<unsigned int>(string.substr(0, pos));
    unsigned int month = boost::lexical_cast<unsigned int>(string.substr(pos + 1, pos2 - pos - 1));
    unsigned int year = boost::lexical_cast<unsigned int>(string.substr(pos2 + 1));
    if (year < 2000 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31) {
	return false;
    }

    entry->year = (uint8_t) (year - 2000);
    entry->month = (uint8_t) month;
    entry->day = (uint8_t) day;

    return true;
}

void
CommandConnection::sendCommand(uint8_t dest, uint8_t type,
			       const uint8_t *data, size_t count,
			       bool expectResponse)
{
    EmsMessage msg(dest, type, std::vector<uint8_t>(data, data + count), expectResponse);

    scheduleResponseTimeout();
    m_handler.getHandler().sendMessage(msg);
}
