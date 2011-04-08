#include <iostream>
#include <mysql++/exceptions.h>
#include <mysql++/query.h>
#include <mysql++/ssqls.h>
#include "Database.h"
#include "Options.h"

const char * Database::dbName = "ems_data";
const char * Database::numericTableName = "numeric_data";
const char * Database::booleanTableName = "boolean_data";
const char * Database::stateTableName = "state_data";

sql_create_4(NumericSensorValue, 1, 4,
	     mysqlpp::sql_smallint, sensor,
	     mysqlpp::sql_float, value,
	     mysqlpp::sql_datetime, starttime,
	     mysqlpp::sql_datetime, endtime);
sql_create_4(BooleanSensorValue, 1, 4,
	     mysqlpp::sql_smallint, sensor,
	     mysqlpp::sql_bool, value,
	     mysqlpp::sql_datetime, starttime,
	     mysqlpp::sql_datetime, endtime);
sql_create_4(StateSensorValue, 1, 4,
	     mysqlpp::sql_smallint, sensor,
	     mysqlpp::sql_varchar, value,
	     mysqlpp::sql_datetime, starttime,
	     mysqlpp::sql_datetime, endtime);

Database::Database() :
    m_connection(NULL)
{
}

Database::~Database()
{
    if (m_connection) {
	delete m_connection;
    }
}

bool
Database::connect(const std::string& server, const std::string& user, const std::string& password)
{
    bool success = false;

    m_connection = new mysqlpp::Connection();
    m_connection->set_option(new mysqlpp::ReconnectOption(true));

    if (!m_connection->connect(NULL, server.c_str(), user.c_str(), password.c_str())) {
	return false;
    }

    NumericSensorValue::table(numericTableName);
    BooleanSensorValue::table(booleanTableName);
    StateSensorValue::table(stateTableName);

    try {
	m_connection->select_db(dbName);
	success = true;
    } catch (mysqlpp::DBSelectionFailed& e) {
	/* DB not yet there, need to create it */
	try {
	    m_connection->create_db(dbName);
	    m_connection->select_db(dbName);
	    success = true;
	} catch (mysqlpp::Exception& e) {
	    std::cerr << "Could not create database: " << e.what() << std::endl;
	}
    }

    if (success) {
	success = createTables();
    }

    return success;
}

bool
Database::createTables()
{
    try {
	mysqlpp::Query query = m_connection->query();
	
	query << "show tables";

	mysqlpp::StoreQueryResult res = query.store();
	if (res && res.num_rows() > 0) {
	    std::cout << "show tables returned " << res.num_rows() << " rows" << std::endl;
	    /* tables already present */
	    return true;
	}

	/* Create sensor list table */
	query << "CREATE TABLE IF NOT EXISTS sensors ("
	      << "  type SMALLINT UNSIGNED NOT NULL, "
	      << "  value_type TINYINT UNSIGNED NOT NULL, "
	      << "  name VARCHAR(100) NOT NULL, "
	      << "  reading_type TINYINT UNSIGNED, "
	      << "  unit VARCHAR(10), "
	      << "  `precision` TINYINT UNSIGNED, "
	      << "  PRIMARY KEY (type)) "
	      << "ENGINE MyISAM CHARACTER SET utf8";
	query.execute();

	/* insert sensor data (id, type, name, unit) */
	createSensorRows();

	/* Create numeric sensor data table */
	query << "CREATE TABLE IF NOT EXISTS " << numericTableName << " ("
	      << "  id INT AUTO_INCREMENT, "
	      << "  sensor SMALLINT UNSIGNED NOT NULL, "
	      << "  value FLOAT NOT NULL, "
	      << "  starttime DATETIME NOT NULL, "
	      << "  endtime DATETIME NOT NULL, "
	      << "  PRIMARY KEY (id), "
	      << "  KEY sensor_starttime (sensor, starttime), "
	      << "  KEY sensor_endtime (sensor, endtime)) "
	      << "ENGINE MyISAM PACK_KEYS 1 ROW_FORMAT DYNAMIC";
	query.execute();

	/* Create boolean sensor data table */
	query << "CREATE TABLE IF NOT EXISTS " << booleanTableName << " ("
	      << "  id INT AUTO_INCREMENT, "
	      << "  sensor SMALLINT UNSIGNED NOT NULL, "
	      << "  value TINYINT NOT NULL, "
	      << "  starttime DATETIME NOT NULL, "
	      << "  endtime DATETIME NOT NULL, "
	      << "  PRIMARY KEY (id), "
	      << "  KEY sensor_starttime (sensor, starttime), "
	      << "  KEY sensor_endtime (sensor, endtime)) "
	      << "ENGINE MyISAM PACK_KEYS 1 ROW_FORMAT DYNAMIC";
	query.execute();

	/* Create state sensor data table */
	query << "CREATE TABLE IF NOT EXISTS " << stateTableName << " ("
	      << "  id INT AUTO_INCREMENT, "
	      << "  sensor SMALLINT UNSIGNED NOT NULL, "
	      << "  value VARCHAR(100) NOT NULL, "
	      << "  starttime DATETIME NOT NULL, "
	      << "  endtime DATETIME NOT NULL, "
	      << "  PRIMARY KEY (id), "
	      << "  KEY sensor_starttime (sensor, starttime), "
	      << "  KEY sensor_endtime (sensor, endtime)) "
	      << "ENGINE MyISAM PACK_KEYS 1 ROW_FORMAT DYNAMIC";
	query.execute();
    } catch (const mysqlpp::BadQuery& er) {
	std::cerr << "Query error: " << er.what() << std::endl;
	return false;
    } catch (const mysqlpp::BadConversion& er) {
	std::cerr << "Conversion error: " << er.what() << std::endl
		  << "\tretrieved data size: " << er.retrieved
		  << ", actual size: " << er.actual_size << std::endl;
	return false;
    } catch (const mysqlpp::Exception& er) {
	std::cerr << std::endl << "Error: " << er.what() << std::endl;
	return false;
    }

    return true;
}

void
Database::createSensorRows()
{
    mysqlpp::Query query = m_connection->query();

    query << "insert into sensors values (%0q, %1q, %2q, %3q:reading_type, %4q:unit, %5q:precision)";
    query.parse();
    query.template_defaults["unit"] = mysqlpp::null;
    query.template_defaults["reading_type"] = mysqlpp::null;
    query.template_defaults["precision"] = mysqlpp::null;

    /* Numeric sensors */
    query.execute(SensorKesselSollTemp, sensorTypeNumeric,
		  "Kessel-Soll-Temperatur", readingTypeTemperature, "°C", 0);
    query.execute(SensorKesselIstTemp, sensorTypeNumeric,
		  "Kessel-Ist-Temperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorWarmwasserSollTemp, sensorTypeNumeric,
		  "Warmwasser-Soll-Temperatur", readingTypeTemperature, "°C", 0);
    query.execute(SensorWarmwasserIstTemp, sensorTypeNumeric,
		  "Warmwasser-Ist-Temperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorVorlaufHK1SollTemp, sensorTypeNumeric,
		  "Vorlauf HK1-Soll-Temperatur", readingTypeTemperature, "°C", 0);
    query.execute(SensorVorlaufHK1IstTemp, sensorTypeNumeric,
		  "Vorlauf HK1-Ist-Temperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorVorlaufHK2SollTemp, sensorTypeNumeric,
		  "Vorlauf HK2-Soll-Temperatur", readingTypeTemperature, "°C", 0);
    query.execute(SensorVorlaufHK2IstTemp, sensorTypeNumeric,
		  "Vorlauf HK2-Ist-Temperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorMischersteuerung, sensorTypeNumeric,
		  "Mischersteuerung", readingTypeNone, "", 0);
    query.execute(SensorRuecklaufTemp, sensorTypeNumeric,
		  "Rücklauftemperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorAussenTemp, sensorTypeNumeric,
		  "Außentemperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorGedaempfteAussenTemp, sensorTypeNumeric,
		  "Gedämpfte Außentemperatur", readingTypeTemperature, "°C", 0);
    query.execute(SensorRaumSollTemp, sensorTypeNumeric,
		  "Raum-Soll-Temperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorRaumIstTemp, sensorTypeNumeric,
		  "Raum-Ist-Temperatur", readingTypeTemperature, "°C", 1);
    query.execute(SensorMomLeistung, sensorTypeNumeric,
		  "Momentane Leistung", readingTypePercent, "%", 0);
    query.execute(SensorMaxLeistung, sensorTypeNumeric,
		  "Maximale Leistung", readingTypePercent, "%", 0);
    query.execute(SensorFlammenstrom, sensorTypeNumeric,
		  "Flammenstrom", readingTypeCurrent, "µA", 1);
    query.execute(SensorSystemdruck, sensorTypeNumeric,
		  "Systemdruck", readingTypePressure, "bar", 1);
    query.execute(SensorBrennerstarts, sensorTypeNumeric,
		  "Brennerstarts", readingTypeCount, "");
    query.execute(SensorBetriebszeit, sensorTypeNumeric,
		  "Betriebszeit", readingTypeTime, "min");
    query.execute(SensorHeizZeit, sensorTypeNumeric,
		  "Heizzeit", readingTypeTime, "min");
    query.execute(SensorWarmwasserbereitungsZeit, sensorTypeNumeric,
		  "Warmwasserbereitungszeit", readingTypeTime, "min");
    query.execute(SensorWarmwasserBereitungen, sensorTypeNumeric,
		  "Warmwasserbereitungen", readingTypeCount, "");

    /* Boolean sensors */
    query.execute(SensorFlamme, sensorTypeBoolean, "Flamme");
    query.execute(SensorBrenner, sensorTypeBoolean, "Brenner");
    query.execute(SensorZuendung, sensorTypeBoolean, "Zündung");
    query.execute(SensorKesselPumpe, sensorTypeBoolean, "Kessel-Pumpe");
    query.execute(Sensor3WegeVentil, sensorTypeBoolean, "3-Wege-Ventil");
    query.execute(SensorZirkulation, sensorTypeBoolean, "Zirkulation");
    query.execute(SensorWarmwasserBereitung, sensorTypeBoolean, "Warmwasserbereitung");
    query.execute(SensorWWTagbetrieb, sensorTypeBoolean, "WW-Tagbetrieb");
    query.execute(SensorSommerbetrieb, sensorTypeBoolean, "Sommerbetrieb");
    query.execute(SensorWarmwasserTempOK, sensorTypeBoolean, "Warmwassertemperatur OK");
    query.execute(SensorWWVorrang, sensorTypeBoolean, "Warmwasservorrang");
    query.execute(SensorHK1Automatik, sensorTypeBoolean, "HK1 Automatikbetrieb");
    query.execute(SensorHK1Tagbetrieb, sensorTypeBoolean, "HK1 Tagbetrieb");
    query.execute(SensorHK1Pumpe, sensorTypeBoolean, "HK1 Pumpe");
    query.execute(SensorHK1Ferien, sensorTypeBoolean, "HK1 Ferien");
    query.execute(SensorHK1Party, sensorTypeBoolean, "HK1 Party");
    query.execute(SensorHK2Automatik, sensorTypeBoolean, "HK2 Automatikbetrieb");
    query.execute(SensorHK2Tagbetrieb, sensorTypeBoolean, "HK2 Tagbetrieb");
    query.execute(SensorHK2Pumpe, sensorTypeBoolean, "HK2 Pumpe");
    query.execute(SensorHK2Ferien, sensorTypeBoolean, "HK2 Ferien");
    query.execute(SensorHK2Party, sensorTypeBoolean, "HK2 Party");

    /* State sensors */
    query.execute(SensorServiceCode, sensorTypeState, "Servicecode");
    query.execute(SensorFehlerCode, sensorTypeState, "Fehlercode");
}

bool
Database::checkAndUpdateRateLimit(unsigned int sensor, time_t now)
{
    std::map<unsigned int, time_t>::iterator iter;
    int limit = Options::rateLimit();

    if (limit == 0) {
	/* no rate limitation */
	return true;
    }

    iter = m_lastWrites.find(sensor);
    if (iter != m_lastWrites.end()) {
	time_t difference = now - iter->second;
	if (difference < limit) {
	    return false;
	}
    }

    m_lastWrites[sensor] = now;
    return true;
}

bool
Database::executeQuery(mysqlpp::Query& query)
{
    try {
	query.execute();
	return true;
    } catch (const mysqlpp::BadQuery& e) {
	std::cerr << "MySQL query error: " << e.what() << std::endl;
    } catch (const mysqlpp::Exception& e) {
	std::cerr << "MySQL exception: " << e.what() << std::endl;
    }

    return false;
}

void
Database::addSensorValue(NumericSensors sensor, float value)
{
    time_t now = time(NULL);
    if (!m_connection || !checkAndUpdateRateLimit(sensor, now)) {
	return;
    }

    mysqlpp::Query query = m_connection->query();
    std::map<unsigned int, float>::iterator cacheIter = m_numericCache.find(sensor);
    std::map<unsigned int, mysqlpp::ulonglong>::iterator idIter = m_lastInsertIds.find(sensor);
    bool valueChanged = cacheIter == m_numericCache.end() || cacheIter->second != value;
    bool idValid = idIter != m_lastInsertIds.end() && idIter->second != 0;
    mysqlpp::sql_datetime timestamp(now);

    if (idValid) {
	query << "update " << numericTableName << " set endtime ='" << timestamp
	      << "' where id = " << idIter->second;
	executeQuery(query);
    }

    if (valueChanged || !idValid) {
	NumericSensorValue row(sensor, value, timestamp, timestamp);
	query.insert(row);
	if (executeQuery(query)) {
	    m_lastInsertIds[sensor] = query.insert_id();
	    m_numericCache[sensor] = value;
	}
    }
}

void
Database::addSensorValue(BooleanSensors sensor, bool value)
{
    time_t now = time(NULL);
    if (!m_connection) {
	return;
    }

    mysqlpp::Query query = m_connection->query();
    std::map<unsigned int, bool>::iterator cacheIter = m_booleanCache.find(sensor);
    std::map<unsigned int, mysqlpp::ulonglong>::iterator idIter = m_lastInsertIds.find(sensor);
    bool valueChanged = cacheIter == m_booleanCache.end() || cacheIter->second != value;
    bool idValid = idIter != m_lastInsertIds.end() && idIter->second != 0;
    mysqlpp::sql_datetime timestamp(now);

    if (idValid) {
	query << "update " << booleanTableName << " set endtime ='" << timestamp
	      << "' where id = " << idIter->second;
	executeQuery(query);
    }

    if (valueChanged || !idValid) {
	BooleanSensorValue row(sensor, value, timestamp, timestamp);
	query.insert(row);
	if (executeQuery(query)) {
	    m_lastInsertIds[sensor] = query.insert_id();
	    m_booleanCache[sensor] = value;
	}
    }
}

void
Database::addSensorValue(StateSensors sensor, const std::string& value)
{
    time_t now = time(NULL);
    if (!m_connection) {
	return;
    }

    mysqlpp::Query query = m_connection->query();
    std::map<unsigned int, std::string>::iterator cacheIter = m_stateCache.find(sensor);
    std::map<unsigned int, mysqlpp::ulonglong>::iterator idIter = m_lastInsertIds.find(sensor);
    bool valueChanged = cacheIter == m_stateCache.end() || cacheIter->second != value;
    bool idValid = idIter != m_lastInsertIds.end() && idIter->second != 0;
    mysqlpp::sql_datetime timestamp(now);

    if (idValid) {
	query << "update " << stateTableName << " set endtime ='" << timestamp
	      << "' where id = " << idIter->second;
	executeQuery(query);
    }

    if (valueChanged || !idValid) {
	StateSensorValue row(sensor, value, timestamp, timestamp);
	query.insert(row);
	if (executeQuery(query)) {
	    m_lastInsertIds[sensor] = query.insert_id();
	    m_stateCache[sensor] = value;
	}
    }
}

