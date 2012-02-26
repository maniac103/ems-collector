#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include "CommandHandler.h"

CommandHandler::CommandHandler(boost::asio::io_service& ioService,
			       boost::asio::ip::tcp::socket& cmdSocket,
			       boost::asio::ip::tcp::endpoint& endpoint) :
    m_service(ioService),
    m_cmdSocket(cmdSocket),
    m_acceptor(ioService, endpoint)
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
    CommandConnection::Ptr connection(new CommandConnection(*this, m_service, m_cmdSocket));
    m_acceptor.async_accept(connection->socket(),
		            boost::bind(&CommandHandler::handleAccept, this,
					connection, boost::asio::placeholders::error));
}


CommandConnection::CommandConnection(CommandHandler& handler,
				     boost::asio::io_service& ioService,
				     boost::asio::ip::tcp::socket& cmdSocket) :
    m_socket(ioService),
    m_cmdSocket(cmdSocket),
    m_handler(handler)
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

    if (m_request.size() > 2) {
	CommandResult result = handleCommand(requestStream);

	switch (result) {
	    case Ok:
		respond("OK");
		break;
	    case Failed:
		respond("ERRFAIL");
		break;
	    case InvalidCmd:
	    case InvalidArgs:
		respond("ERRCMD");
		break;
	    case Waiting:
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
	return handleGetErrorsCommand();
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleHkCommand(std::istream& request, uint8_t base)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	std::vector<uint8_t> data = { 0x10, base, 0x07 };
	std::string mode;

	request >> mode;

	if (mode == "day") {
	    data.push_back(0x01);
	} else if (mode == "night") {
	    data.push_back(0x00);
	} else if (mode == "auto") {
	    data.push_back(0x02);
	} else {
	    return InvalidArgs;
	}
	return sendCommand(data);
    } else if (cmd == "daytemperature") {
	return handleHkTemperatureCommand(request, base, 0x02);
    } else if (cmd == "nighttemperature") {
	return handleHkTemperatureCommand(request, base, 0x01);
    } else if (cmd == "holidaytemperature") {
	return handleHkTemperatureCommand(request, base, 0x07);
    } else if (cmd == "holidaymode") {
	/* TODO */
	return Failed;
    } else if (cmd == "vacationmode") {
	/* TODO */
	return Failed;
    } else if (cmd == "partymode") {
	std::vector<uint8_t> data = { 0x10, base, 0x56 };
	unsigned int hours;

	request >> hours;

	if (!request || hours > 99) {
	    return InvalidArgs;
	}
	data.push_back(hours);
	return sendCommand(data);
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleHkTemperatureCommand(std::istream& request, uint8_t base, uint8_t cmd)
{
    std::vector<uint8_t> data = { 0x10, base, cmd };
    float value;
    uint8_t valueByte;

    request >> value;
    if (request) {
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

    data.push_back(valueByte);
    return sendCommand(data);
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
	std::vector<uint8_t> data = { 0x10, 0x37, 0x02 };
	std::string mode;

	request >> mode;

	if (mode == "on") {
	    data.push_back(0x01);
	} else if (mode == "off") {
	    data.push_back(0x00);
	} else if (mode == "auto") {
	    data.push_back(0x02);
	} else {
	    return InvalidArgs;
	}
	return sendCommand(data);
    } else if (cmd == "temperature") {
	std::vector<uint8_t> data = { 0x08, 0x33, 0x02 };
	unsigned int temperature;

	request >> temperature;

	if (!request || temperature < 30 || temperature > 60) {
	    return InvalidArgs;
	}
	data.push_back(temperature);

	return sendCommand(data);
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleThermDesinfectCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	std::vector<uint8_t> data = { 0x10, 0x37, 0x04 };
	std::string mode;

	request >> mode;

	if (mode == "on") {
	    data.push_back(0xff);
	} else if (mode == "off") {
	    data.push_back(0x00);
	} else {
	    return InvalidArgs;
	}
	return sendCommand(data);
    } else if (cmd == "day") {
	std::vector<uint8_t> data = { 0x10, 0x37, 0x05 };
	std::string day;

	request >> day;

	if (day == "monday") {
	    data.push_back(0x00);
	} else if (day == "tuesday") {
	    data.push_back(0x01);
	} else if (day == "wednesday") {
	    data.push_back(0x02);
	} else if (day == "thursday") {
	    data.push_back(0x03);
	} else if (day == "friday") {
	    data.push_back(0x04);
	} else if (day == "saturday") {
	    data.push_back(0x05);
	} else if (day == "sunday") {
	    data.push_back(0x06);
	} else if (day == "everyday") {
	    data.push_back(0x07);
	} else {
	    return InvalidArgs;
	}
	return sendCommand(data);
    } else if (cmd == "temperature") {
	std::vector<uint8_t> data = { 0x08, 0x33, 0x08 };
	unsigned int temperature;

	request >> temperature;

	if (!request || temperature < 60 || temperature > 80) {
	    return InvalidArgs;
	}
	data.push_back(temperature);

	return sendCommand(data);
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleZirkPumpCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	std::vector<uint8_t> data = { 0x10, 0x37, 0x03 };
	std::string mode;

	request >> mode;

	if (mode == "on") {
	    data.push_back(0x01);
	} else if (mode == "off") {
	    data.push_back(0x00);
	} else if (mode == "auto") {
	    data.push_back(0x02);
	} else {
	    return InvalidArgs;
	}
	return sendCommand(data);
    } else if (cmd == "count") {
	std::vector<uint8_t> data = { 0x08, 0x33, 0x07 };
	std::string countString;

	request >> countString;

	if (countString == "alwayson") {
	    data.push_back(0x07);
	} else {
	    try {
		unsigned int count = boost::lexical_cast<unsigned int>(countString);
		if (count < 1 || count > 6) {
		    return InvalidArgs;
		}
		data.push_back(count);
	    } catch (boost::bad_lexical_cast& e) {
		return InvalidArgs;
	    }
	}
	return sendCommand(data);
    }

    return InvalidCmd;
}

CommandConnection::CommandResult
CommandConnection::handleGetErrorsCommand()
{
    std::vector<uint8_t> data = { 0x10, 0x12 };
    CommandResult result = sendCommand(data);

    if (result != Ok) {
	return result;
    }

    /* TODO: redirect response to here
     *
     * response example:
     * 0x35, 0x31, 0x03, 0x2e, 0x89, 0x08, 0x16, 0x1c, 0x0d, 0x00, 0x00, 0x30
     *
     * 0x35 0x31 -> A51
     * 0x03 0x2e -> 0x32e -> error code 814
     * 0x89 -> 0x80 = has date, 0x09 = 2009
     * 0x08 -> August
     * 0x16 -> hour 22
     * 0x1c -> day 28
     * 0x0d -> minute 13
     * 0x00 0x00 -> 0x0 -> error still ongoing (otherwise duration)
     * 0x30 -> error source
     */

    return Waiting;
}

void
CommandConnection::handlePcMessage(const EmsMessage& message)
{
}

CommandConnection::CommandResult
CommandConnection::sendCommand(const std::vector<uint8_t>& data)
{
    boost::system::error_code error;
    boost::asio::write(m_cmdSocket, boost::asio::buffer(data), error);

    if (error) {
	std::cerr << "Command send error: " << error.message() << std::endl;
	return Failed;
    }

    return Ok;
}
