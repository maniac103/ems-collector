#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <vector>
#include <ostream>
#include "Database.h"

class Message
{
    public:
	Message(Database& db, size_t size) :
	    m_db(db),
	    m_buffer(size),
	    m_pos(0),
	    m_fill(0),
	    m_csum(0) { }

	void addData(unsigned char data) {
	    m_buffer[m_fill++] = data;
	    m_csum ^= data;
	}

	bool isFull() {
	    return m_fill == m_buffer.size();
	}
	bool checksumMatches(unsigned char csum) {
	    return m_csum == csum;
	}
	virtual void parse() {
	    assert(m_fill == m_buffer.size());
	}

    protected:
	Database& m_db;
	std::vector<unsigned char> m_buffer;

    private:
	size_t m_pos;
	size_t m_fill;
	unsigned char m_csum;
};

class DataMessage : public Message {
    public:
	DataMessage(Database& db, size_t size) :
	    Message(db, size) {}
	virtual void parse();

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

	void printNumberAndAddToDb(size_t offset, size_t size, int divider,
				   const char *name, const char *unit,
				   Database::NumericSensors sensor);
	void printBoolAndAddToDb(int byte, int bit, const char *name,
				 Database::BooleanSensors sensor);
	void printStateAndAddToDb(const std::string& value, const char *name,
				  Database::StateSensors sensor);
};

class StatsMessage : public Message {
    public:
	StatsMessage(Database& db, size_t size) :
	    Message(db, size) {}
	virtual void parse();
};

#endif /* __MESSAGE_H__ */
