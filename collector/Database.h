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
	    SensorKesselIstTemp = 2,
	    SensorWarmwasserSollTemp = 3,
	    SensorWarmwasserIstTemp = 4,
	    SensorVorlaufHK1SollTemp = 5,
	    SensorVorlaufHK1IstTemp = 6,
	    SensorVorlaufHK2SollTemp = 7,
	    SensorVorlaufHK2IstTemp = 8,
	    SensorMischersteuerung = 9,
	    SensorRuecklaufTemp = 10,
	    SensorAussenTemp = 11,
	    SensorGedaempfteAussenTemp = 12,
	    SensorRaumSollTemp = 13,
	    SensorRaumIstTemp = 14,
	    SensorMomLeistung = 15,
	    SensorMaxLeistung = 16,
	    SensorFlammenstrom = 17,
	    SensorSystemdruck = 18,
	    SensorBetriebszeit = 19,
	    SensorHeizZeit = 23,
	    SensorBrennerstarts = 20,
	    SensorWarmwasserbereitungsZeit = 21,
	    SensorWarmwasserBereitungen = 22,
	    /* not valid for DB */
	    NumericSensorLast = 24
	} NumericSensors;

	typedef enum {
	    SensorFlamme = 100,
	    SensorBrenner = 101,
	    SensorZuendung = 102,
	    SensorHKPumpe = 103,
	    SensorHKWW = 106,
	    SensorHK1Active = 104,
	    SensorHK2Active = 105,
	    SensorHK1Pumpe = 116,
	    SensorHK2Pumpe = 117,
	    SensorWarmwasserBereitung = 110,
	    SensorWarmwasserTempOK = 114,
	    SensorZirkulation = 107,
	    SensorWWVorrang = 115,
	    SensorAutomatikbetrieb = 111,
	    SensorTagbetrieb = 112,
	    SensorSommerbetrieb = 113,
	    /* not valid for DB */
	    BooleanSensorLast = 118
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
