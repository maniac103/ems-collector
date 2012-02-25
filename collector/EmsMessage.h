#ifndef __EMSMESSAGE_H__
#define __EMSMESSAGE_H__

#include <vector>
#include <ostream>
#include "Database.h"

class EmsMessage
{
    public:
	EmsMessage(Database& db, const std::vector<uint8_t>& data);
	void handle();

    public:
	static const uint8_t addressUBA  = 0x08;
	static const uint8_t addressBC10 = 0x09;
	static const uint8_t addressPC   = 0x0b;
	static const uint8_t addressRC   = 0x10;
	static const uint8_t addressWM10 = 0x11;
	static const uint8_t addressMM10 = 0x21;

	uint8_t getSource() {
	    return m_source;
	}
	uint8_t getDestination() {
	    return m_dest & 0x7f;
	}

    private:
	void parseUBAMonitorFastMessage();
	void parseUBAMonitorSlowMessage();
	void parseUBAMonitorWWMessage();
	void parseUBAUnknown1Message();

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
	Database& m_db;
	std::vector<unsigned char> m_data;
	uint8_t m_source;
	uint8_t m_dest;
	uint8_t m_type;
};

#endif /* __EMSMESSAGE_H__ */
