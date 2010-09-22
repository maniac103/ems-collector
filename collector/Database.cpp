#include <iostream>
#include <mysql++/exceptions.h>
#include <mysql++/query.h>
#include <mysql++/ssqls.h>
#include "Database.h"
#include "Options.h"

const char * Database::dbName = "ems_data";

sql_create_3(NumericSensorValue, 1, 3,
	     mysqlpp::sql_smallint, sensor,
	     mysqlpp::sql_float, value,
	     mysqlpp::sql_timestamp, time);
sql_create_3(BooleanSensorValue, 1, 3,
	     mysqlpp::sql_smallint, sensor,
	     mysqlpp::sql_bool, value,
	     mysqlpp::sql_timestamp, time);

Database::Database() :
    m_connection(),
    m_transaction(NULL)
{
}

Database::Database(const std::string& server, const std::string& user, const std::string& password) :
    m_connection(NULL, server.c_str(), user.c_str(), password.c_str()),
    m_transaction(NULL)
{
    bool success = false;

    NumericSensorValue::table("numeric_data");
    BooleanSensorValue::table("boolean_data");

    try {
	m_connection.select_db(dbName);
	success = true;
    } catch (mysqlpp::DBSelectionFailed& e) {
	/* DB not yet there, need to create it */
	try {
	    m_connection.create_db(dbName);
	    m_connection.select_db(dbName);
	    if (createTables()) {
		success = true;
	    } else {
		m_connection.drop_db(dbName);
	    }
	} catch (mysqlpp::Exception& e) {
	    std::cerr << "Could not create database: " << e.what() << std::endl;
	}
    }

    if (!success) {
	m_connection.disconnect();
    }
}

Database::~Database()
{
    finishTransaction(false);
}

bool
Database::createTables()
{
    try {
	mysqlpp::Query query = m_connection.query();

	/* Create sensor list table */
	query << "CREATE TABLE sensors ("
	      << "  type SMALLINT UNSIGNED NOT NULL, "
	      << "  value_type TINYINT UNSIGNED NOT NULL, "
	      << "  name VARCHAR(100) NOT NULL, "
	      << "  reading_type TINYINT UNSIGNED, "
	      << "  unit VARCHAR(10), "
	      << "  precision TINYINT UNSIGNED, "
	      << "  PRIMARY KEY (type)) "
	      << "ENGINE MyISAM CHARACTER SET utf8";
	query.execute();

	/* insert sensor data (id, type, name, unit) */
	createSensorRows();

	/* Create numeric sensor data table */
	query << "CREATE TABLE numeric_data ("
	      << "  sensor SMALLINT UNSIGNED NOT NULL, "
	      << "  value FLOAT NOT NULL, "
	      << "  time TIMESTAMP NOT NULL, "
	      << "  KEY time (time), "
	      << "  KEY sensor_time (sensor, time)) "
	      << "ENGINE MyISAM PACK_KEYS 1 ROW_FORMAT DYNAMIC";
	query.execute();

	/* Create boolean sensor data table */
	query << "CREATE TABLE boolean_data ("
	      << "  sensor SMALLINT UNSIGNED NOT NULL, "
	      << "  value TINYINT NOT NULL, "
	      << "  time TIMESTAMP NOT NULL, "
	      << "  KEY time (time), "
	      << "  KEY sensor_time (sensor, time)) "
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
    mysqlpp::Transaction transaction(m_connection);
    mysqlpp::Query query = m_connection.query();

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
		  "Betriebszeit", readingTypeTime, "h");
    query.execute(SensorWarmwasserbereitungsZeit, sensorTypeNumeric,
		  "Warmwasserbereitungszeit", readingTypeTime, "h");
    query.execute(SensorWarmwasserBereitungen, sensorTypeNumeric,
		  "Warmwasserbereitungen", readingTypeCount, "");

    /* Boolean sensors */
    query.execute(SensorFlamme, sensorTypeBoolean, "Flamme");
    query.execute(SensorBrenner, sensorTypeBoolean, "Brenner");
    query.execute(SensorZuendung, sensorTypeBoolean, "Zündung");
    query.execute(SensorHKPumpe, sensorTypeBoolean, "HK Pumpe");
    query.execute(SensorHKWW, sensorTypeBoolean, "HK/WW");
    query.execute(SensorZirkulation, sensorTypeBoolean, "Zirkulation");
    query.execute(Sensor3WegeHeizen, sensorTypeBoolean, "3-Wege-Ventil Heizen");
    query.execute(Sensor3WegeWW, sensorTypeBoolean, "3-Wege-Ventil Warmwasser");
    query.execute(SensorWarmwasserBereitung, sensorTypeBoolean, "Warmwasserbereitung");
    query.execute(SensorAutomatikbetrieb, sensorTypeBoolean, "Automatikbetrieb");
    query.execute(SensorTagbetrieb, sensorTypeBoolean, "Tagbetrieb");
    query.execute(SensorSommerbetrieb, sensorTypeBoolean, "Sommerbetrieb");
    query.execute(SensorWarmwasserTempOK, sensorTypeBoolean, "Warmwassertemperatur OK");
    query.execute(SensorHK1Active, sensorTypeBoolean, "HK1 aktiv");
    query.execute(SensorHK2Active, sensorTypeBoolean, "HK2 aktiv");

    transaction.commit();
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

void
Database::addSensorValue(NumericSensors sensor, float value)
{
    time_t now = time(NULL);
    if (!isConnected() || !checkAndUpdateRateLimit(sensor, now)) {
	return;
    }

    mysqlpp::Query query = m_connection.query();
    NumericSensorValue row(sensor, value, mysqlpp::sql_timestamp(now));

    query.insert(row);
    query.execute();
}

void
Database::addSensorValue(BooleanSensors sensor, bool value)
{
    time_t now = time(NULL);
    if (!isConnected() || !checkAndUpdateRateLimit(sensor, now)) {
	return;
    }

    mysqlpp::Query query = m_connection.query();
    BooleanSensorValue row(sensor, value, mysqlpp::sql_timestamp(now));

    query.insert(row);
    query.execute();
}

void
Database::finishTransaction(bool commit)
{
    if (m_transaction) {
	if (isConnected() && commit) {
	    m_transaction->commit();
	}
	delete m_transaction;
	m_transaction = NULL;
    }
}

