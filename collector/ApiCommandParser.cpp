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

#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include "ApiCommandParser.h"
#include "ByteOrder.h"
#include "Options.h"

/* version of our command API */
#define API_VERSION "2016030701"

ApiCommandParser::ApiCommandParser(EmsCommandSender& sender,
				   IncomingMessageHandler& msgHandler,
				   const boost::shared_ptr<EmsCommandClient>& client,
				   ValueCache *cache,
				   OutputCallback outputCb) :
    m_sender(sender),
    m_msgHandler(msgHandler),
    m_client(client),
    m_cache(cache),
    m_outputCb(outputCb),
    m_responseCounter(0),
    m_parsePosition(0),
    m_outputRawData(false)
{
}

static const char * scheduleNames[] = {
    "custom1", "family", "morning", "early", "evening", "forenoon",
    "afternoon", "noon", "single", "senior", "custom2"
};
static const size_t scheduleNameCount = sizeof(scheduleNames) / sizeof(scheduleNames[0]);

static const char * dayNames[] = {
    "monday", "tuesday", "wednesday", "thursday",
    "friday", "saturday", "sunday"
};
static const size_t dayNameCount = sizeof(dayNames) / sizeof(dayNames[0]);

ApiCommandParser::CommandResult
ApiCommandParser::parse(std::istream& request)
{
    if (m_activeRequest) {
	return Busy;
    }

    std::string category;
    request >> category;

    if (category == "help") {
	output("Available commands (help with '<command> help'):\n"
		"hk[1|2|3|4]\n"
		"ww\n"
		"uba\n"
		"rc\n"
#if defined(HAVE_RAW_READWRITE_COMMAND)
		"raw\n"
#endif
		"cache\n"
		"getversion\n"
		"OK");
	return Ok;
    } else if (category == "hk1") {
	return handleHkCommand(request, 61);
    } else if (category == "hk2") {
	return handleHkCommand(request, 71);
    } else if (category == "hk3") {
	return handleHkCommand(request, 81);
    } else if (category == "hk4") {
	return handleHkCommand(request, 91);
    } else if (category == "ww") {
	return handleWwCommand(request);
    } else if (category == "rc") {
	return handleRcCommand(request);
    } else if (category == "uba") {
	return handleUbaCommand(request);
#if defined (HAVE_RAW_READWRITE_COMMAND)
    } else if (category == "raw") {
	return handleRawCommand(request);
#endif
    } else if (category == "cache") {
	return handleCacheCommand(request);
    } else if (category == "getversion") {
	output("collector version: " API_VERSION);
	startRequest(EmsProto::addressUBA, 0x02, 0, 3);
	return Ok;
    }

    return InvalidCmd;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleRcCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "help") {
	output("Available subcommands:\n"
		"mintemperature <temp>\n"
		"buildingtype [light|medium|heavy]\n"
		"outdoortempdamping [on|off]\n"
		"requestdata\n"
		"geterrors\n"
		"getcontactinfo\n"
		"setcontactinfo [1|2] <text>\n"
		"settime YYYY-MM-DD HH:MM:SS\n"
		"OK");
	return Ok;
    } else if (cmd == "requestdata") {
	startRequest(EmsProto::addressRC3x, 0xa5, 0, 25);
	return Ok;
    } else if (cmd == "mintemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, 0xa5, 5, 1, -30, 0);
    } else if (cmd == "buildingtype") {
	std::string ns;
	uint8_t data;

	request >> ns;

	if (ns == "light")       data = 0;
	else if (ns == "medium") data = 1;
	else if (ns == "heavy")  data = 2;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0xa5, 6, &data, 1);
	return Ok;
    } else if (cmd == "outdoortempdamping") {
	uint8_t data;
	std::string mode;

	request >> mode;

	if (mode == "on")       data = 0xff;
	else if (mode == "off") data = 0x00;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0xa5, 21, &data, 1);
	return Ok;
    } else if (cmd == "getcontactinfo") {
	startRequest(EmsProto::addressRC3x, 0xa4, 0, 42);
	return Ok;
    } else if (cmd == "setcontactinfo") {
	unsigned int line;
	std::ostringstream buffer;
	std::string text;

	request >> line;
	if (!request || line < 1 || line > 2) {
	    return InvalidArgs;
	}

	while (request) {
	    std::string token;
	    request >> token;
	    buffer << token << " ";
	}

	// make sure there's at least 21 characters in there
	buffer << "                     ";

	text = buffer.str().substr(0, 21);
	sendCommand(EmsProto::addressRC3x, 0xa4, (line - 1) * 21, (uint8_t *) text.c_str(), 21);
	return Ok;
    } else if (cmd == "geterrors") {
	startRequest(EmsProto::addressRC3x, 0x12, 0, 4 * sizeof(EmsProto::ErrorRecord));
	return Ok;
    } else if (cmd == "settime") {
	std::locale prevLocale = request.imbue(std::locale(std::locale::classic(),
		new boost::local_time::local_time_input_facet("%Y-%m-%d %H:%M:%S")));
	boost::posix_time::ptime time;

	request >> time;
	request.imbue(prevLocale);

	if (!request) {
	    return InvalidArgs;
	}

	boost::gregorian::date date = time.date();
	boost::posix_time::time_duration timeOfDay = time.time_of_day();

	EmsProto::SystemTimeRecord record;
	memset(&record, 0, sizeof(record));

	record.common.year = date.year() - 2000;
	record.common.month = date.month();
	record.common.day = date.day();
	record.common.hour = timeOfDay.hours();
	record.common.minute = timeOfDay.minutes();
	record.second = timeOfDay.seconds();
	switch (date.day_of_week()) {
	    case boost::date_time::Monday: record.dayOfWeek = 0; break;
	    case boost::date_time::Tuesday: record.dayOfWeek = 1; break;
	    case boost::date_time::Wednesday: record.dayOfWeek = 2; break;
	    case boost::date_time::Thursday: record.dayOfWeek = 3; break;
	    case boost::date_time::Friday: record.dayOfWeek = 4; break;
	    case boost::date_time::Saturday: record.dayOfWeek = 5; break;
	    case boost::date_time::Sunday: record.dayOfWeek = 6; break;
	}

	sendCommand(EmsProto::addressRC3x, 0x06, 0, (uint8_t *) &record, sizeof(record));
	return Ok;
    }

    return InvalidCmd;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleUbaCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "help") {
	output("Available subcommands:\n"
		"antipendel <minutes>\n"
		"hyst [on|off] <kelvin>\n"
		"burnermodulation <minpercent> <maxpercent>\n"
		"pumpmodulation <minpercent> <maxpercent>\n"
		"pumpdelay <minutes>\n"
		"geterrors\n"
		"schedulemaintenance [off | byhour <hours / 100> | bydate YYYY-MM-DD]\n"
		"checkmaintenanceneeded\n"
		"testmode [on|off] <burnerpercent> <pumppercent> <3wayonww:[0|1]> <zirkpump:[0|1]>\n"
		"requestdata\n"
		"OK");
	return Ok;
    } else if (cmd == "requestdata") {
	startRequest(EmsProto::addressUBA, 0x15, 0, 5);
	return Ok;
    } else if (cmd == "geterrors") {
	startRequest(EmsProto::addressUBA, 0x10, 0, 8 * sizeof(EmsProto::ErrorRecord));
	return Ok;
    } else if (cmd == "antipendel") {
	uint8_t minutes;
	if (!parseIntParameter(request, minutes, 120)) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressUBA, 0x16, 6, &minutes, 1);
	return Ok;
    } else if (cmd == "hyst") {
	std::string direction;

	request >> direction;

	if (direction == "on") {
	    return handleSingleByteValue(request, EmsProto::addressUBA, 0x16, 5, 1, -20, -1);
	} else if (direction == "off") {
	    return handleSingleByteValue(request, EmsProto::addressUBA, 0x16, 4, 1, 1, 20);
	}

	return InvalidArgs;
    } else if (cmd == "burnermodulation") {
	unsigned int min, max;
	uint8_t data[2];

	request >> min >> max;
	if (!request || min > max || max > 100) {
	    return InvalidArgs;
	}

	data[0] = max;
	data[1] = min;

	sendCommand(EmsProto::addressUBA, 0x16, 2, data, sizeof(data));
	return Ok;
    } else if (cmd == "pumpmodulation") {
	unsigned int min, max;
	uint8_t data[2];

	request >> min >> max;
	if (!request || min > max || max > 100) {
	    return InvalidArgs;
	}

	data[0] = max;
	data[1] = min;

	sendCommand(EmsProto::addressUBA, 0x16, 9, data, sizeof(data));
	return Ok;
    } else if (cmd == "pumpdelay") {
	uint8_t minutes;
	if (!parseIntParameter(request, minutes, 120)) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressUBA, 0x16, 8, &minutes, 1);
	return Ok;
    } else if (cmd == "schedulemaintenance") {
	std::string kind;
	uint8_t data[5] = { 0, 60, 1, 1, 4 };

	request >> kind;
	if (kind == "bydate") {
	    std::string due;
	    EmsProto::HolidayEntry dueDate;

	    request >> due;
	    if (!request || !parseHolidayEntry(due, &dueDate)) {
		return InvalidArgs;
	    }

	    data[0] = 2;
	    data[2] = dueDate.day;
	    data[3] = dueDate.month;
	    data[4] = dueDate.year;
	} else if (kind == "byhours") {
	    uint8_t hours;
	    if (!parseIntParameter(request, hours, 60)) {
		return InvalidArgs;
	    }

	    data[0] = 1;
	    data[1] = hours;
	} else if (kind == "off") {
	    /* initializer is sufficient */
	} else {
	    return InvalidArgs;
	}

	sendCommand(EmsProto::addressUBA, 0x15, 0, data, sizeof(data));
	return Ok;
    } else if (cmd == "checkmaintenanceneeded") {
	startRequest(EmsProto::addressUBA, 0x1c, 5, 3);
	return Ok;
    } else if (cmd == "testmode") {
	std::string mode;
	uint8_t data[11];

	memset(&data, 0, sizeof(data));
	request >> mode;

	if (mode == "on") {
	    unsigned int brennerPercent, pumpePercent;
	    bool threeWayOn, zirkPumpOn;

	    request >> brennerPercent;
	    if (!request || brennerPercent > 100) {
		return InvalidArgs;
	    }
	    request >> pumpePercent;
	    if (!request || pumpePercent > 100) {
		return InvalidArgs;
	    }
	    request >> threeWayOn >> zirkPumpOn;
	    if (!request) {
		return InvalidArgs;
	    }

	    data[0] = 0x5a;
	    data[1] = brennerPercent;
	    data[3] = pumpePercent;
	    data[4] = threeWayOn ? 0xff : 0;
	    data[5] = zirkPumpOn ? 0xff : 0;
	} else if (mode != "off") {
	    return InvalidArgs;
	}

	sendCommand(EmsProto::addressUBA, 0x1d, 0, data, sizeof(data));
	return Ok;
    }

    return InvalidCmd;
}

#if defined(HAVE_RAW_READWRITE_COMMAND)
ApiCommandParser::CommandResult
ApiCommandParser::handleRawCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "help") {
	output("Available subcommands:\n"
		"read <target> <type> <offset> <len>\n"
		"write <target> <type> <offset> <data>\n"
		"OK");
	return Ok;
    } else if (cmd == "read") {
	uint8_t target, type, offset, len;
	if (!parseIntParameter(request, target, UCHAR_MAX)     ||
		!parseIntParameter(request, type, UCHAR_MAX)   ||
		!parseIntParameter(request, offset, UCHAR_MAX) ||
		!parseIntParameter(request, len, UCHAR_MAX)) {
	    return InvalidArgs;
	}
	startRequest(target, type, offset, len, true, true);
	return Ok;
    } else if (cmd == "write") {
	uint8_t target, type, offset, value;
	if (!parseIntParameter(request, target, UCHAR_MAX)     ||
		!parseIntParameter(request, type, UCHAR_MAX)   ||
		!parseIntParameter(request, offset, UCHAR_MAX) ||
		!parseIntParameter(request, value, UCHAR_MAX)) {
	    return InvalidArgs;
	}

	sendCommand(target, type, offset, &value, 1);
	return Ok;
    }

    return InvalidCmd;
}
#endif

ApiCommandParser::CommandResult
ApiCommandParser::handleCacheCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (m_cache) {
	if (cmd == "help") {
	    output("Available subcommands:\n"
		   "fetch <key>\n"
		   "OK");
	    return Ok;
	} else if (cmd == "fetch") {
	    std::ostringstream stream;
	    std::vector<std::string> selector;

	    while (request) {
		std::string token;
		request >> token;
		if (!token.empty()) {
		    selector.push_back(token);
		}
	    }

	    m_cache->outputValues(selector, stream);
	    output(stream.str());
	    output("OK");
	    return Ok;
	}
    }

    return InvalidCmd;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleHkCommand(std::istream& request, uint8_t type)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "help") {
	output("Available subcommands:\n"
		"mode [day|night|auto]\n"
		"daytemperature <temp>\n"
		"nighttemperature <temp>\n"
		"temperatureoverride <temp>\n"
		"getholiday\n"
		"holidaymode <start:YYYY-MM-DD> <end:YYYY-MM-DD>\n"
		"vacationtemperature <temp>\n"
		"getvacation\n"
		"vacationmode <start:YYYY-MM-DD> <end:YYYY-MM-DD>\n"
		"partymode <hours>\n"
		"pausemode <hours>\n"
		"getactiveschedule\n"
		"selectschedule [family|morning|early|evening|forenoon|noon|afternoon|single|senior|custom1|custom2]\n"
		"getcustomschedule [1|2]\n"
		"customschedule [1|2] <index> unset\n"
		"customschedule [1|2] <index> [monday|tuesday|...|sunday] HH:MM [on|off]\n"
		"scheduleoptimizer [on|off]\n"
		"mintemperature <temp>\n"
		"maxtemperature <temp>\n"
		"reductionmode [offmode|reduced|raumhalt|aussenhalt]\n"
		"heatingsystem [none|heater|floorheater|convection] [outdoor|indoor]\n"
		"vacationreductionmode [outdoor|indoor]\n"
		"maxroomeffect <temp>\n"
		"designtemperature <temp>\n"
		"roomtemperatureoffset <temp>\n"
		"frostprotectmode [off|byoutdoortemp|byindoortemp]\n"
		"frostprotecttemperature <temp>\n"
		"summerwinterthreshold <temp>\n"
		"reducedmodethreshold <temp>\n"
		"vacationreducedmodethreshold <temp>\n"
		"cancelreducedmodethreshold <temp>\n"
		"requestdata\n"
		"OK");
	return Ok;
    } else if (cmd == "requestdata") {
	startRequest(EmsProto::addressRC3x, type, 0, 42);
	return Ok;
    } else if (cmd == "mode") {
	uint8_t data;
	std::string mode;

	request >> mode;

	if (mode == "day")        data = 0x01;
	else if (mode == "night") data = 0x00;
	else if (mode == "auto")  data = 0x02;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, type, 7, &data, 1);
	return Ok;
    } else if (cmd == "daytemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 2, 2, 5, 30);
    } else if (cmd == "nighttemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 1, 2, 5, 30);
    } else if (cmd == "temperatureoverride") {
	uint8_t data;
	std::string value;

	request >> value;

	if (value == "off") {
	    data = 0;
	} else {
	    try {
		float floatValue = boost::lexical_cast<float>(value);
		if (floatValue < 5 || floatValue > 30) {
		    return InvalidArgs;
		}
		data = boost::numeric_cast<uint8_t>(2 * floatValue);
	    } catch (boost::bad_lexical_cast& e) {
		return InvalidArgs;
	    } catch (boost::bad_numeric_cast& e) {
		return InvalidArgs;
	    }
	}

	sendCommand(EmsProto::addressRC3x, type, 37, &data, 1);
	return Ok;
    } else if (cmd == "vacationtemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 3, 2, 5, 30);
    } else if (cmd == "holidaymode") {
	return handleSetHolidayCommand(request, type + 2, 93);
    } else if (cmd == "vacationmode") {
	return handleSetHolidayCommand(request, type + 2, 87);
    } else if (cmd == "partymode") {
	uint8_t hours;
	if (!parseIntParameter(request, hours, 99)) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressRC3x, type + 2, 86, &hours, 1);
	return Ok;
    } else if (cmd == "pausemode") {
	uint8_t hours;
	if (!parseIntParameter(request, hours, 99)) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressRC3x, type + 2, 85, &hours, 1);
	return Ok;
    } else if (cmd == "customschedule") {
	unsigned int schedule, index;
	EmsProto::ScheduleEntry entry;

	request >> schedule >> index;

	if (!request || schedule < 1 || schedule > 2 || index > 42 || !parseScheduleEntry(request, &entry)) {
	    return InvalidArgs;
	}

	sendCommand(EmsProto::addressRC3x, type + (schedule - 1) * 3 + 2,
		(index - 1) * sizeof(EmsProto::ScheduleEntry),
		(uint8_t *) &entry, sizeof(entry));
	return Ok;
    } else if (cmd == "getcustomschedule") {
	unsigned int schedule;

	request >> schedule;
	if (!request || schedule < 1 || schedule > 2) {
	    return InvalidArgs;
	}

	startRequest(EmsProto::addressRC3x, type + (schedule - 1) * 3 + 2,
		0, 42 * sizeof(EmsProto::ScheduleEntry));
	return Ok;
    } else if (cmd == "getactiveschedule") {
	startRequest(EmsProto::addressRC3x, type + 2, 84, 1);
	return Ok;
    } else if (cmd == "selectschedule") {
	std::string schedule;
	uint8_t data;

	request >> schedule;

	for (data = 0; data < scheduleNameCount; data++) {
	    if (schedule == scheduleNames[data]) {
		break;
	    }
	}
	if (data == scheduleNameCount) {
	    return InvalidArgs;
	}

	sendCommand(EmsProto::addressRC3x, type + 2, 84, &data, 1);
	return Ok;
    } else if (cmd == "getvacation") {
	startRequest(EmsProto::addressRC3x, type + 2, 87, 2 * sizeof(EmsProto::HolidayEntry));
	return Ok;
    } else if (cmd == "getholiday") {
	startRequest(EmsProto::addressRC3x, type + 2, 93, 2 * sizeof(EmsProto::HolidayEntry));
	return Ok;
    } else if (cmd == "scheduleoptimizer") {
	std::string value;
	uint8_t data;

	request >> value;

	if (value == "on")       data = 0xff;
	else if (value == "off") data = 0x00;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, type, 19, &data, 1);
	return Ok;
    } else if (cmd == "reductionmode") {
	std::string ns;
	uint8_t data;

	request >> ns;

	if (ns == "offmode")         data = 0;
	else if (ns == "reduced")    data = 1;
	else if (ns == "raumhalt")   data = 2;
	else if (ns == "aussenhalt") data = 3;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, type, 25, &data, 1);
	return Ok;
    } else if (cmd == "heatingsystem") {
	Options::RoomControllerType rcType = Options::roomControllerType();
	std::string system, controlType;
	uint8_t systemData, controlData;

	request >> system >> controlType;

	if (system == "none")             systemData = 0;
	else if (system == "heater")      systemData = 1;
	else if (system == "floorheater") systemData = 2;
	else if (system == "convection")  systemData = 3;
	else return InvalidArgs;

	if (controlType == "outdoor")     controlData = 0;
	else if (controlType == "indoor") controlData = 1;
	else return InvalidArgs;

	if (rcType == Options::RC30) {
	    if (controlData == 1) {
		systemData = 4; // Raumvorlauf
	    }
	    sendCommand(EmsProto::addressRC3x, type, 0, &systemData, 1);
	    return Ok;
	} else if (rcType == Options::RC35) {
	    uint8_t data[] = { systemData, controlData };
	    sendCommand(EmsProto::addressRC3x, type, 32, data, sizeof(data));
	    return Ok;
	}
	return InvalidCmd;
    } else if (cmd == "vacationreductionmode") {
	std::string ns;
	uint8_t data;

	request >> ns;

	if (ns == "outdoor")     data = 3;
	else if (ns == "indoor") data = 2;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, type, 41, &data, 1);
	return Ok;
    } else if (cmd == "frostprotectmode") {
	std::string ns;
	uint8_t data;

	request >> ns;

	if (ns == "off")                data = 0;
	else if (ns == "byoutdoortemp") data = 1;
	else if (ns == "byindoortemp")  data = 2;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, type, 28, &data, 1);
	return Ok;
    } else if (cmd == "mintemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 16, 1, 5, 70);
    } else if (cmd == "maxtemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 15, 1, 30, 90);
    } else if (cmd == "maxroomeffect") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 4, 2, 0, 10);
    } else if (cmd == "roomtemperatureoffset") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 6, 2, -5, 5);
    } else if (cmd == "designtemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 17, 1, 30, 90);
    } else if (cmd == "frostprotecttemperature") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 23, 1, -20, 10);
    } else if (cmd == "summerwinterthreshold") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 22, 1, 0, 30);
    } else if (cmd == "reducedmodethreshold") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 39, 1, -20, 10);
    } else if (cmd == "vacationreducedmodethreshold") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 40, 1, -20, 10);
    } else if (cmd == "cancelreducedmodethreshold") {
	return handleSingleByteValue(request, EmsProto::addressRC3x, type, 38, 1, -31, 10);
    }
    return InvalidCmd;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleSingleByteValue(std::istream& request, uint8_t dest, uint8_t type,
					 uint8_t offset, int multiplier, int min, int max)
{
    float value;
    int valueInt;
    int8_t valueByte;

    request >> value;
    if (!request) {
	return InvalidArgs;
    }

    try {
	valueInt = boost::numeric_cast<int>(multiplier * value);
	if (valueInt < min * multiplier || valueInt > max * multiplier) {
	    return InvalidArgs;
	}
	valueByte = valueInt;
    } catch (boost::numeric::bad_numeric_cast& e) {
	return InvalidArgs;
    }

    sendCommand(dest, type, offset, (uint8_t *) &valueByte, 1);
    return Ok;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleSetHolidayCommand(std::istream& request, uint8_t type, uint8_t offset)
{
    std::string beginString, endString;
    EmsProto::HolidayEntry entries[2];
    EmsProto::HolidayEntry *begin = entries;
    EmsProto::HolidayEntry *end = entries + 1;

    request >> beginString;
    request >> endString;

    if (!request) {
	return InvalidArgs;
    }

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

    sendCommand(EmsProto::addressRC3x, type, offset, (uint8_t *) entries, sizeof(entries));
    return Ok;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleWwCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "help") {
	output("Available subcommands:\n"
	        "mode [on|off|auto]\n"
		"temperature <temp>\n"
		"limittemperature <temp>\n"
		"loadonce\n"
		"cancelload\n"
		"getcustomschedule\n"
		"customschedule <index> unset\n"
		"customschedule <index> [monday|tuesday|...|sunday] HH:MM [on|off]\n"
		"selectschedule [custom|hk]\n"
		"showloadindicator [on|off]\n"
		"thermdesinfect mode [on|off]\n"
		"thermdesinfect day [monday|tuesday|...|sunday]\n"
		"thermdesinfect hour <hour>\n"
		"thermdesinfect temperature <temp>\n"
		"zirkpump mode [on|off|auto]\n"
		"zirkpump count [1|2|3|4|5|6|alwayson]\n"
		"zirkpump getcustomschedule\n"
		"zirkpump customschedule <index> unset\n"
		"zirkpump customschedule <index> [monday|tuesday|...|sunday] HH:MM [on|off]\n"
		"zirkpump selectschedule [custom|hk]\n"
		"requestdata\n"
		"OK");
	return Ok;
    } else if (cmd == "thermdesinfect") {
	return handleThermDesinfectCommand(request);
    } else if (cmd == "zirkpump") {
	return handleZirkPumpCommand(request);
    } else if (cmd == "mode") {
	uint8_t data;
	std::string mode;

	request >> mode;

	if (mode == "on")        data = 0x01;
	else if (mode == "off")  data = 0x00;
	else if (mode == "auto") data = 0x02;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0x37, 2, &data, 1);
	return Ok;
    } else if (cmd == "temperature") {
	uint8_t temperature;
	if (!parseIntParameter(request, temperature, 80) || temperature < 30) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressUBA, 0x33, 2, &temperature, 1);
	return Ok;
    } else if (cmd == "limittemperature") {
	uint8_t temperature;
	if (!parseIntParameter(request, temperature, 80) || temperature < 30) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressRC3x, 0x37, 8, &temperature, 1);
	return Ok;
    } else if (cmd == "loadonce") {
	uint8_t data = 35;
	sendCommand(EmsProto::addressUBA, 0x35, 0, &data, 1);
	return Ok;
    } else if (cmd == "cancelload") {
	uint8_t data = 3;
	sendCommand(EmsProto::addressUBA, 0x35, 0, &data, 1);
	return Ok;
    } else if (cmd == "showloadindicator") {
	uint8_t data;
	std::string mode;

	request >> mode;

	if (mode == "on")       data = 0xff;
	else if (mode == "off") data = 0x00;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0x37, 9, &data, 1);
	return Ok;
    } else if (cmd == "getcustomschedule") {
	startRequest(EmsProto::addressRC3x, 0x38, 0, 42 * sizeof(EmsProto::ScheduleEntry));
	return Ok;
    } else if (cmd == "customschedule") {
	unsigned int index;
	EmsProto::ScheduleEntry entry;

	request >> index;

	if (!request || index > 42 || !parseScheduleEntry(request, &entry)) {
	    return InvalidArgs;
	}

	sendCommand(EmsProto::addressRC3x, 0x38,
		(index - 1) * sizeof(EmsProto::ScheduleEntry),
		(uint8_t *) &entry, sizeof(entry));
	return Ok;
    } else if (cmd == "selectschedule") {
	std::string schedule;
	uint8_t data;

	request >> schedule;

	if (schedule == "custom")  data = 0xff;
	else if (schedule == "hk") data = 0x00;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0x37, 0, &data, 1);
	return Ok;
    } else if (cmd == "requestdata") {
	startRequest(EmsProto::addressUBA, 0x33, 0, 10);
	return Ok;
    }

    return InvalidCmd;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleThermDesinfectCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	uint8_t data;
	std::string mode;

	request >> mode;

	if (mode == "on")       data = 0xff;
	else if (mode == "off") data = 0x00;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0x37, 4, &data, 1);
	return Ok;
    } else if (cmd == "day") {

	uint8_t data;
	std::string day;

	request >> day;

	if (day == "everyday") {
	    data = 7;
	} else {
	    for (data = 0; data < dayNameCount; data++) {
		if (day == dayNames[data]) {
		    break;
		}
	    }
	    if (data == dayNameCount) {
		return InvalidArgs;
	    }
	}

	sendCommand(EmsProto::addressRC3x, 0x37, 5, &data, 1);
	return Ok;
    } else if (cmd == "hour") {
	uint8_t hour;
	if (!parseIntParameter(request, hour, 23)) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressRC3x, 0x37, 6, &hour, 1);
	return Ok;
    } else if (cmd == "temperature") {
	uint8_t temperature;
	if (!parseIntParameter(request, temperature, 80) || temperature < 60) {
	    return InvalidArgs;
	}
	sendCommand(EmsProto::addressUBA, 0x33, 8, &temperature, 1);
	return Ok;
    }

    return InvalidCmd;
}

ApiCommandParser::CommandResult
ApiCommandParser::handleZirkPumpCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	uint8_t data;
	std::string mode;

	request >> mode;

	if (mode == "on")        data = 0x01;
	else if (mode == "off")  data = 0x00;
	else if (mode == "auto") data = 0x02;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0x37, 3, &data, 1);
	return Ok;
    } else if (cmd == "count") {
	uint8_t count;
	std::string countString;

	request >> countString;

	if (countString == "alwayson") {
	    count = 0x07;
	} else {
	    try {
		count = boost::lexical_cast<unsigned int>(countString);
		if (count < 1 || count > 6) {
		    return InvalidArgs;
		}
	    } catch (boost::bad_lexical_cast& e) {
		return InvalidArgs;
	    }
	}
	sendCommand(EmsProto::addressUBA, 0x33, 7, &count, 1);
	return Ok;
    } else if (cmd == "getcustomschedule") {
	startRequest(EmsProto::addressRC3x, 0x39, 0, 42 * sizeof(EmsProto::ScheduleEntry));
	return Ok;
    } else if (cmd == "customschedule") {
	unsigned int index;
	EmsProto::ScheduleEntry entry;

	request >> index;

	if (!request || index > 42 || !parseScheduleEntry(request, &entry)) {
	    return InvalidArgs;
	}

	sendCommand(EmsProto::addressRC3x, 0x39,
		(index - 1) * sizeof(EmsProto::ScheduleEntry),
		(uint8_t *) &entry, sizeof(entry));
	return Ok;
    } else if (cmd == "selectschedule") {
	std::string schedule;
	uint8_t data;

	request >> schedule;

	if (schedule == "custom")  data = 0xff;
	else if (schedule == "hk") data = 0x00;
	else return InvalidArgs;

	sendCommand(EmsProto::addressRC3x, 0x37, 1,  &data, 1);
	return Ok;
    }

    return InvalidCmd;
}

boost::tribool
ApiCommandParser::onIncomingMessage(const EmsMessage& message)
{
    if (!m_activeRequest) {
	return boost::indeterminate;
    }

    const std::vector<uint8_t>& data = message.getData();
    uint8_t source = message.getSource();
    uint8_t type = message.getType();
    uint8_t offset = message.getOffset();

    if (type == 0xff) {
	bool success = offset != 0x04;
	if (success) {
	    EmsMessage simulatedResponse(0, // simulate broadcast
					 m_activeRequest->getDestination(),
					 m_activeRequest->getType(),
					 m_activeRequest->getOffset(),
					 m_activeRequest->getData(),
					 false);
	    m_msgHandler.handleIncomingMessage(simulatedResponse.getSendData(false));
	}
	m_activeRequest.reset();
	return success;
    }

    if (source != m_requestDestination ||
	    type != m_requestType      ||
	    offset != (m_requestResponse.size() + m_requestOffset)) {
	/* likely a response to a request we already retried, ignore it */
	return boost::indeterminate;
    }

    if (data.empty()) {
	// no more data is available
	m_requestLength = m_requestResponse.size();
    } else {
	m_requestResponse.insert(m_requestResponse.end(), data.begin(), data.end());
    }

    boost::tribool result;

    if (m_outputRawData) {
	if (!continueRequest()) {
	    std::ostringstream outputStream;
	    for (size_t i = 0; i < m_requestResponse.size(); i++) {
		outputStream << boost::format("0x%02x ") % (unsigned int) m_requestResponse[i];
	    }
	    output(outputStream.str());
	    result = true;
	} else {
	    result = boost::indeterminate;
	}
    } else {
	result = handleResponse();
    }

    if (result) {
	m_activeRequest.reset();
    }
    return result;
}

boost::tribool
ApiCommandParser::handleResponse()
{
    switch (m_requestType) {
	case 0x02: /* get version */ {
	    static const struct {
		uint8_t source;
		const char *name;
	    } SOURCES[] = {
		{ EmsProto::addressUBA, "UBA" },
		{ EmsProto::addressBC10, "BC10" },
		{ EmsProto::addressRC3x, "RC3x" }
	    };
	    static const size_t SOURCECOUNT = sizeof(SOURCES) / sizeof(SOURCES[0]);

	    unsigned int major = m_requestResponse[1];
	    unsigned int minor = m_requestResponse[2];
	    size_t index;

	    for (index = 0; index < SOURCECOUNT; index++) {
		if (m_requestDestination == SOURCES[index].source) {
		    boost::format f("%s version: %d.%02d");
		    f % SOURCES[index].name % major % minor;
		    output(f.str());
		    break;
		}
	    }
	    if (index >= (SOURCECOUNT - 1)) {
		return true;
	    }
	    startRequest(SOURCES[index + 1].source, 0x02, 0, 3);
	    break;
	}
	case 0x10: /* get locking UBA errors */
	case 0x11: /* get blocking UBA errors */
	case 0x12: /* get active RC errors */
	case 0x13: /* get deleted RC errors */ {
	    static const char * errorTypes[] = {
		"L", "B", "S", "D",
	    };
	    const char *prefix = errorTypes[m_requestType - 0x10];
	    boost::tribool result = loopOverResponse<EmsProto::ErrorRecord>(prefix);
	    if (result == true && (m_requestType == 0x10 || m_requestType == 0x12)) {
		unsigned int count = m_requestType == 0x10 ? 5 : 4;
		startRequest(m_requestDestination, m_requestType + 1, 0,
			count * sizeof(EmsProto::ErrorRecord), false);
	    } else {
		return result;
	    }
	    break;
	}
	case 0x15: /* get maintenance parameters */
	    startRequest(EmsProto::addressUBA, 0x16, 0, 20);  /* get uba parameters */
	    break;
	case 0x16: /* get uba parameters */
	    return true;
	case 0x1c: /* check for maintenance */
	    switch (m_requestResponse[0]) {
		case 0: output("not due"); break;
		case 3: output("due: hours"); break;
		case 8: output("due: date"); break;
	    }
	    return true;
	case 0x3d: /* get opmode HK1 */
	case 0x47: /* get opmode HK2 */
	case 0x51: /* get opmode HK3 */
	case 0x5b: /* get opmode HK4 */
	    if (!continueRequest()) {
		startRequest(EmsProto::addressRC3x, m_requestType + 1, 0, 20, false);
	    }
	    break;
	case 0x3e: /* HK1 status 2 */
	case 0x48: /* HK2 status 2 */
	case 0x52: /* HK3 status 2 */
	case 0x5c: /* HK4 status 2 */
	    /* finally get party/pause info */
	    startRequest(EmsProto::addressRC3x, m_requestType + 1, 85, 2, false);
	    break;
	case 0x3f: /* get schedule 1 HK1 */
	case 0x42: /* get schedule 2 HK1 */
	case 0x49: /* get schedule 1 HK2 */
	case 0x4c: /* get schedule 2 HK2 */
	case 0x53: /* get schedule 1 HK3 */
	case 0x56: /* get schedule 2 HK3 */
	case 0x5d: /* get schedule 1 HK4 */
	case 0x60: /* get schedule 2 HK4 */
	    if (m_requestOffset == 84) {
		/* 'get active schedule' response */
		const char *name = "unknown";
		for (size_t i = 0; i < scheduleNameCount; i++) {
		    if (m_requestResponse[0] == i) {
			name = scheduleNames[i];
			break;
		    }
		}
		output(name);
		return true;
	    } else if (m_requestOffset == 85) {
		/* get party/pause info request */
		return true;
	    } else if (m_requestOffset > 80) {
		/* it's at the end -> holiday schedule */
		const size_t msgSize = sizeof(EmsProto::HolidayEntry);

		if (m_requestResponse.size() < 2 * msgSize) {
		    return false;
		}

		EmsProto::HolidayEntry *begin = (EmsProto::HolidayEntry *) &m_requestResponse.at(0);
		EmsProto::HolidayEntry *end = (EmsProto::HolidayEntry *) &m_requestResponse.at(msgSize);
		output(buildRecordResponse("begin", begin));
		output(buildRecordResponse("end", end));
		return true;
	    } else {
		/* it's at the beginning -> heating schedule */
		return loopOverResponse<EmsProto::ScheduleEntry>();
	    }
	    break;
	case 0x38: /* get WW schedule */
	case 0x39: /* get WW ZP schedule */
	    return loopOverResponse<EmsProto::ScheduleEntry>();
	case 0x33: /* requestdata WW part 1 */
	    startRequest(EmsProto::addressUBA, 0x34, 0, 12); // get part 2
	    break;
	case 0x34: /* requestdata WW part 2 */
	    startRequest(EmsProto::addressRC3x, 0x37, 0, 12); // get part 3
	    break;
	case 0x37: /* requestdata WW part 3 */
	    return true; // finished requesting WW data
	    break;
	case 0xa4: { /* get contact info */
	    if (!continueRequest()) {
		for (size_t i = 0; i < m_requestResponse.size(); i += 21) {
		    size_t len = std::min(m_requestResponse.size() - i, static_cast<size_t>(21));
		    char buffer[22];
		    memcpy(buffer, &m_requestResponse.at(i), len);
		    buffer[len] = 0;
		    output(buffer);
		}
		return true;
	    }
	    break;
	}
	case 0xa5: /* get system parameters */
	    return true;
	default:
	    /* unhandled message */
	    return false;
    }

    return boost::indeterminate;
}

template<typename T> boost::tribool
ApiCommandParser::loopOverResponse(const char *prefix)
{
    const size_t msgSize = sizeof(T);
    while (m_parsePosition + msgSize <= m_requestResponse.size()) {
	T *record = (T *) &m_requestResponse.at(m_parsePosition);
	std::string response = buildRecordResponse(record);

	m_parsePosition += msgSize;
	m_responseCounter++;

	if (response.empty()) {
	    return true;
	}

	boost::format f("%s%02d %s");
	f % prefix % m_responseCounter % response;
	output(f.str());
    }

    if (!continueRequest()) {
	return true;
    }

    return boost::indeterminate;
}

bool
ApiCommandParser::onTimeout()
{
    if (!m_activeRequest) {
	return false;
    }
    m_retriesLeft--;
    if (m_retriesLeft == 0) {
	m_activeRequest.reset();
	return true;
    }

    sendActiveRequest();
    return false;
}

std::string
ApiCommandParser::buildRecordResponse(const EmsProto::ErrorRecord *record)
{
    if (record->errorAscii[0] == 0) {
	/* no error at this position */
	return "";
    }

    std::ostringstream response;

    if (record->time.valid) {
	response << boost::format("%04d-%02d-%02d %02d:%02d")
		% (2000 + record->time.year) % (unsigned int) record->time.month
		% (unsigned int) record->time.day % (unsigned int) record->time.hour
		% (unsigned int) record->time.minute;
    } else {
	response  << "xxxx-xx-xx xx:xx";
    }

    response << " ";
    response << boost::format("%02x %c%c %d %d")
	    % (unsigned int) record->source % record->errorAscii[0] % record->errorAscii[1]
	    % BE16_TO_CPU(record->code_be16) % BE16_TO_CPU(record->durationMinutes_be16);

    return response.str();
}

std::string
ApiCommandParser::buildRecordResponse(const EmsProto::ScheduleEntry *entry)
{
    if (entry->time >= 0x90) {
	/* unset */
	return "";
    }

    unsigned int minutes = entry->time * 10;
    boost::format f("%s %02d:%02d %s");
    f % dayNames[entry->day / 2] % (minutes / 60) % (minutes % 60) % (entry->on ? "on" : "off");

    return f.str();
}

bool
ApiCommandParser::parseScheduleEntry(std::istream& request, EmsProto::ScheduleEntry *entry)
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

    if (mode == "on") {
	entry->on = 1;
    } else if (mode == "off") {
	entry->on = 0;
    } else {
	return false;
    }

    bool hasDay = false;
    for (size_t i = 0; i < dayNameCount; i++) {
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
    try {
	unsigned int hours = boost::lexical_cast<unsigned int>(time.substr(0, pos));
	unsigned int minutes = boost::lexical_cast<unsigned int>(time.substr(pos + 1));
	if (hours > 23 || minutes >= 60 || (minutes % 10) != 0) {
	    return false;
	}

	entry->time = (uint8_t) ((hours * 60 + minutes) / 10);
    } catch (boost::bad_lexical_cast& e) {
	return false;
    }

    return true;
}

std::string
ApiCommandParser::buildRecordResponse(const char *type, const EmsProto::HolidayEntry *entry)
{
    boost::format f("%s %04d-%02d-%02d");
    f % type % (2000 + entry->year) % (unsigned int) entry->month % (unsigned int) entry->day;

    return f.str();
}

bool
ApiCommandParser::parseHolidayEntry(const std::string& string, EmsProto::HolidayEntry *entry)
{
    size_t pos = string.find('-');
    if (pos == std::string::npos) {
	return false;
    }

    size_t pos2 = string.find('-', pos + 1);
    if (pos2 == std::string::npos) {
	return false;
    }

    try {
	unsigned int year = boost::lexical_cast<unsigned int>(string.substr(0, pos));
	unsigned int month = boost::lexical_cast<unsigned int>(string.substr(pos + 1, pos2 - pos - 1));
	unsigned int day = boost::lexical_cast<unsigned int>(string.substr(pos2 + 1));
	if (year < 2000 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31) {
	    return false;
	}

	entry->year = (uint8_t) (year - 2000);
	entry->month = (uint8_t) month;
	entry->day = (uint8_t) day;
    } catch (boost::bad_lexical_cast& e) {
	return false;
    }

    return true;
}

void
ApiCommandParser::startRequest(uint8_t dest, uint8_t type, size_t offset,
			        size_t length, bool newRequest, bool raw)
{
    m_requestOffset = offset;
    m_requestLength = length;
    m_requestDestination = dest;
    m_requestType = type;
    m_requestResponse.clear();
    m_requestResponse.reserve(length);
    m_parsePosition = 0;
    m_outputRawData = raw;
    if (newRequest) {
	m_responseCounter = 0;
    }

    continueRequest();
}

bool
ApiCommandParser::continueRequest()
{
    size_t alreadyReceived = m_requestResponse.size();

    if (alreadyReceived >= m_requestLength) {
	return false;
    }

    uint8_t offset = (uint8_t) (m_requestOffset + alreadyReceived);
    uint8_t remaining = (uint8_t) (m_requestLength - alreadyReceived);

    sendCommand(m_requestDestination, m_requestType, offset, &remaining, 1, true);
    return true;
}

void
ApiCommandParser::sendCommand(uint8_t dest, uint8_t type, uint8_t offset,
			       const uint8_t *data, size_t count,
			       bool expectResponse)
{
    std::vector<uint8_t> sendData(data, data + count);

    m_retriesLeft = MaxRequestRetries;
    m_activeRequest.reset(new EmsMessage(dest, type, offset, sendData, expectResponse));

    sendActiveRequest();
}

bool
ApiCommandParser::parseIntParameter(std::istream& request, uint8_t& data, uint8_t max)
{
    unsigned int value;

    request.unsetf(std::ios_base::basefield);
    request >> value;

    if (!request || value > max) {
	return false;
    }

    data = value;
    return true;
}

void
ApiCommandParser::sendActiveRequest()
{
    m_sender.sendMessage(m_client, m_activeRequest);
}
