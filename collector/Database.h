#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <map>
#include <queue>
#include <mysql++/connection.h>
#include <mysql++/query.h>

class Database {
    public:
	Database();
	~Database();

    public:
	bool connect(const std::string& server, const std::string& user, const std::string& password);

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
	    SensorKesselPumpe = 103,
	    /* 0 = HK, 1 = WW */
	    Sensor3WegeVentil = 106,
	    SensorHK1Automatik = 122,
	    SensorHK1Tagbetrieb = 104,
	    SensorHK1Pumpe = 116,
	    SensorHK1Ferien = 118,
	    SensorHK1Party = 119,
	    SensorHK2Automatik = 123,
	    SensorHK2Tagbetrieb = 105,
	    SensorHK2Pumpe = 117,
	    SensorHK2Ferien = 120,
	    SensorHK2Party = 121,
	    SensorWarmwasserBereitung = 110,
	    SensorWarmwasserTempOK = 114,
	    SensorZirkulation = 107,
	    SensorWWVorrang = 115,
	    SensorWWTagbetrieb = 112,
	    SensorSommerbetrieb = 113,
	    /* not valid for DB */
	    BooleanSensorLast = 124
	} BooleanSensors;

	typedef enum {
	    SensorServiceCode = 200,
	    SensorFehlerCode = 201,
	    /* not valid for DB */
	    StateSensorLast = 202
	} StateSensors;

	void addSensorValue(NumericSensors sensor, float value);
	void addSensorValue(BooleanSensors sensor, bool value);
	void addSensorValue(StateSensors sensor, const std::string& value);

    private:
	bool createTables();
	void createSensorRows();
	bool checkAndUpdateRateLimit(unsigned int sensor, time_t now);
	bool executeQuery(mysqlpp::Query& query);

    private:
	static const char *dbName;
	static const char *numericTableName;
	static const char *booleanTableName;
	static const char *stateTableName;

	static const unsigned int sensorTypeNumeric = 1;
	static const unsigned int sensorTypeBoolean = 2;
	static const unsigned int sensorTypeState = 3;

	static const unsigned int readingTypeNone = 0;
	static const unsigned int readingTypeTemperature = 1;
	static const unsigned int readingTypePercent = 2;
	static const unsigned int readingTypeCurrent = 3;
	static const unsigned int readingTypePressure = 4;
	static const unsigned int readingTypeTime = 5;
	static const unsigned int readingTypeCount = 6;

	std::map<unsigned int, time_t> m_lastWrites;
	std::map<unsigned int, float> m_numericCache;
	std::map<unsigned int, bool> m_booleanCache;
	std::map<unsigned int, std::string> m_stateCache;
	std::map<unsigned int, mysqlpp::ulonglong> m_lastInsertIds;
	mysqlpp::Connection *m_connection;
};

#endif /* __DATABASE_H__ */
