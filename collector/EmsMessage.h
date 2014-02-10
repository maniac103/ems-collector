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

#ifndef __EMSMESSAGE_H__
#define __EMSMESSAGE_H__

#include <vector>
#include <ostream>
#include "Database.h"

class EmsMessage
{
    public:
	EmsMessage(Database *db, const std::vector<uint8_t>& data);
	EmsMessage(uint8_t dest, uint8_t type, const std::vector<uint8_t>& data, bool expectResponse);

	void handle();

    public:
	static const uint8_t addressUBA  = 0x08;
	static const uint8_t addressBC10 = 0x09;
	static const uint8_t addressPC   = 0x0b;
	static const uint8_t addressRC   = 0x10;
	static const uint8_t addressWM10 = 0x11;
	static const uint8_t addressMM10 = 0x21;

	uint8_t getSource() const {
	    return m_source;
	}
	uint8_t getDestination() const {
	    return m_dest & 0x7f;
	}
	uint8_t getType() const {
	    return m_type;
	}
	const std::vector<uint8_t>& getData() const {
	    return m_data;
	}
	std::vector<uint8_t> getSendData() const;

    public:
#pragma pack(push,1)
	typedef struct {
	    uint8_t errorAscii[2];
	    uint16_t code_be16;
	    uint8_t year : 7;
	    uint8_t hasDate : 1;
	    uint8_t month;
	    uint8_t hour;
	    uint8_t day;
	    uint8_t minute;
	    uint16_t durationMinutes_be16;
	    uint8_t source;
	} ErrorRecord;

	typedef struct {
	    uint8_t on : 4;
	    uint8_t day : 4;
	    uint8_t time;
	} ScheduleEntry;

	typedef struct {
	    uint8_t day;
	    uint8_t month;
	    uint8_t year;
	} HolidayEntry;
#pragma pack(pop)

    private:
	void parseUBAMonitorFastMessage();
	void parseUBAMonitorSlowMessage();
	void parseUBAMonitorWWMessage();
	void parseUBAParameterWWMessage();
	void parseUBAUnknown1Message();
	void parseUBAErrorMessage();
	void parseUBAParametersMessage();

	void parseRCTimeMessage();
	void parseRCOutdoorTempMessage();
	void parseRCHKMonitorMessage(const char *name,
				     Database::NumericSensors vorlaufSollSensor,
				     Database::BooleanSensors automatikSensor,
				     Database::BooleanSensors tagSensor,
				     Database::BooleanSensors ferienSensor,
				     Database::BooleanSensors partySensor);

	void parseWMTemp1Message();
	void parseWMTemp2Message();

	void parseMMTempMessage();

    private:
	void printNumberAndAddToDb(size_t offset, size_t size, int divider,
				   const char *name, const char *unit,
				   Database::NumericSensors sensor);
	void printBoolAndAddToDb(int byte, int bit, const char *name,
				 Database::BooleanSensors sensor);
	void printStateAndAddToDb(const std::string& value, const char *name,
				  Database::StateSensors sensor);

    private:
	Database *m_db;
	std::vector<unsigned char> m_data;
	uint8_t m_source;
	uint8_t m_dest;
	uint8_t m_type;
};

#endif /* __EMSMESSAGE_H__ */
