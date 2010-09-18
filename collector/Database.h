#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <map>
#include <mysql++/connection.h>
#include <mysql++/transaction.h>

class Database {
    public:
	Database();
	Database(const std::string& server, const std::string& user, const std::string& password);
	~Database();

	bool isConnected() const {
	    return m_connection.connected();
	}

	void startTransaction() {
	    finishTransaction(false);
	    if (isConnected()) {
		m_transaction = new mysqlpp::Transaction(m_connection);
	    }
	}

	void finishTransaction(bool commit = true);

    public:
	typedef enum {
	    SensorKesselSollTemp = 1,
	    SensorKesselIstTemp,
	    SensorWarmwasserSollTemp,
	    SensorWarmwasserIstTemp,
	    SensorVorlaufHK1SollTemp,
	    SensorVorlaufHK1IstTemp,
	    SensorVorlaufHK2SollTemp,
	    SensorVorlaufHK2IstTemp,
	    SensorMischersteuerung,
	    SensorRuecklaufTemp,
	    SensorAussenTemp,
	    SensorGedaempfteAussenTemp,
	    SensorRaumSollTemp,
	    SensorRaumIstTemp,
	    SensorMomLeistung,
	    SensorMaxLeistung,
	    SensorFlammenstrom,
	    SensorSystemdruck,
	    SensorBetriebszeit,
	    SensorBrennerstarts,
	    SensorWarmwasserbereitungsZeit,
	    SensorWarmwasserBereitungen,
	    NumericSensorLast
	} NumericSensors;

	typedef enum {
	    SensorFlamme = 100,
	    SensorBrenner,
	    SensorZuendung,
	    SensorHKPumpe,
	    SensorHK1Pumpe,
	    SensorHK2Pumpe,
	    SensorHKWW,
	    SensorZirkulation,
	    Sensor3WegeHeizen,
	    Sensor3WegeWW,
	    SensorWarmwasserBereitung,
	    SensorAutomatikbetrieb,
	    SensorTagbetrieb,
	    BooleanSensorLast
	} BooleanSensors;

	void addSensorValue(NumericSensors sensor, float value);
	void addSensorValue(BooleanSensors sensor, bool value);

    private:
	bool createTables();
	void createSensorRows();
	bool checkAndUpdateRateLimit(unsigned int sensor, time_t now);

    private:
	static const char *dbName;

	static const unsigned int sensorTypeNumeric = 1;
	static const unsigned int sensorTypeBoolean = 2;

	static const unsigned int readingTypeNone = 0;
	static const unsigned int readingTypeTemperature = 1;
	static const unsigned int readingTypePercent = 2;
	static const unsigned int readingTypeCurrent = 3;
	static const unsigned int readingTypePressure = 4;
	static const unsigned int readingTypeTime = 5;
	static const unsigned int readingTypeCount = 6;

	std::map<unsigned int, time_t> m_lastWrites;
	mysqlpp::Connection m_connection;
	mysqlpp::Transaction *m_transaction;
};

#endif /* __DATABASE_H__ */
