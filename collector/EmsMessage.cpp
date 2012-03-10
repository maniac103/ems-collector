#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include "EmsMessage.h"
#include "Options.h"

#define BYTEFORMAT_HEX \
    "0x" << std::setbase(16) << std::setw(2) << std::setfill('0') << (unsigned int)
#define BYTEFORMAT_DEC \
    std::dec << (unsigned int)

#define RETURN_ON_SIZE_MISMATCH(expected,text)             \
    if (m_data.size() != expected) {                       \
	std::cerr << text << " size mismatch (";           \
	std::cerr << std::dec << m_data.size() << " vs. "; \
	std::cerr << expected << ")" << std::endl;         \
	return;                                            \
    }

EmsMessage::EmsMessage(Database *db, const std::vector<uint8_t>& data) :
    m_db(db),
    m_data(data)
{
    if (m_data.size() >= 3) {
	m_source = m_data[0];
	m_dest = m_data[1];
	m_type = m_data[2];
	m_data.erase(m_data.begin(), m_data.begin() + 3);
    } else {
	m_source = 0;
	m_dest = 0;
	m_type = 0;
    }
}

EmsMessage::EmsMessage(uint8_t dest, uint8_t type,
		       const std::vector<uint8_t>& data,
		       bool expectResponse) :
    m_db(NULL),
    m_data(data),
    m_source(addressPC),
    m_dest(dest | (expectResponse ? 0x80 : 0)),
    m_type(type)
{
}

std::vector<uint8_t>
EmsMessage::getSendData() const
{
    std::vector<uint8_t> data;

    /* own address omitted on send */
    data.push_back(m_dest);
    data.push_back(m_type);
    data.insert(data.end(), m_data.begin(), m_data.end());

    return data;
}

void
EmsMessage::handle()
{
    bool handled = false;
    DebugStream& debug = Options::messageDebug();

    if (debug) {
	time_t now = time(NULL);
	struct tm time;

	localtime_r(&now, &time);
	debug << std::dec << "MESSAGE[";
	debug << std::setw(2) << std::setfill('0') << time.tm_mday;
	debug << "." << std::setw(2) << std::setfill('0') << (time.tm_mon + 1);
	debug << "." << (time.tm_year + 1900) << " ";
	debug << std::setw(2) << std::setfill('0') << time.tm_hour;
	debug << ":" << std::setw(2) << std::setfill('0') << time.tm_min;
	debug << ":" << std::setw(2) << std::setfill('0') << time.tm_sec;
	debug << "]: source " << BYTEFORMAT_HEX m_source;
	debug << ", dest " << BYTEFORMAT_HEX m_dest;
	debug << ", type " << BYTEFORMAT_HEX m_type;
	debug << ", data ";
	for (size_t i = 0; i < m_data.size(); i++) {
	    debug << " " << BYTEFORMAT_HEX m_data[i];
	}
	debug << std::endl;
    }

    if (!m_source && !m_dest && !m_type) {
	/* invalid packet */
	return;
    }

    if (m_dest & 0x80) {
	/* if highest bit of dest is set, it's a polling request -> ignore */
	return;
    }

    switch (m_source) {
	case addressUBA:
	    /* UBA message */
	    switch (m_type) {
		case 0x07:
		    /* yet unknown contents:
		     * 0x8 0x0 0x7 0x0 0x3 0x3 0x0 0x2 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0 */
		    break;
		case 0x10:
		    /* yet unknown:
		     * 0x8 0x10 0x10 0x0 0x34 0x45 0x0 0xe1 0x8a 0x4 0x13 0x1c 0x22 0x0 0x0 0x0 */
		    break;
		case 0x11:
		    /* similar to above:
		     * 0x8 0x10 0x11 0x24 0x32 0x46 0x1 0x4 0x8a 0x3 0x5 0x5 0x1e 0x0 0x0 0x0 */
		    break;
		case 0x14: parseUBAUnknown1Message(); handled = true; break;
		case 0x18: parseUBAMonitorFastMessage(); handled = true; break;
		case 0x19: parseUBAMonitorSlowMessage(); handled = true; break;
		case 0x1c:
		    /* unknown message with varying length
		     * 0x8 0x10 0x1c 0x0 0x8a 0x4 0x13 0x1c 0x1d 0x0 0x0 0x0
		     * 0x8 0x10 0x1c 0x8
		     */
		    break;
		case 0x34: parseUBAMonitorWWMessage(); handled = true; break;
	    }
	    break;
	case addressBC10:
	    /* BC10 message */
	    switch (m_type) {
		case 0x29:
		    /* yet unknown: 0x9 0x10 0x29 0x0 0x6b */
		    break;
	    }
	    break;
	case addressRC:
	    /* RC message */
	    switch (m_type) {
		case 0x06: parseRCTimeMessage(); handled = true; break;
		case 0x1A: /* command for UBA3 */ handled = true; break;
		case 0x35: /* command for UBA3 */ handled = true; break;
		case 0x3E:
		    parseRCHKMonitorMessage("HK1",
					    Database::SensorVorlaufHK1SollTemp,
					    Database::SensorHK1Automatik,
					    Database::SensorHK1Tagbetrieb,
					    Database::SensorHK1Ferien,
					    Database::SensorHK1Party);
		    handled = true;
		    break;
		case 0x48:
		    parseRCHKMonitorMessage("HK2",
					    Database::SensorVorlaufHK2SollTemp,
					    Database::SensorHK2Automatik,
					    Database::SensorHK2Tagbetrieb,
					    Database::SensorHK2Ferien,
					    Database::SensorHK2Party);
		    handled = true;
		    break;
		case 0x9D: /* command for WM10 */ handled = true; break;
		case 0xA2: /* unknown, 11 zeros */ break;
		case 0xA3: parseRCOutdoorTempMessage(); handled = true; break;
		case 0xAC: /* command for MM10 */ handled = true; break;
	    }
	case addressWM10:
	    /* WM10 message */
	    switch (m_type) {
		case 0x9C: parseWMTemp1Message(); handled = true; break;
		case 0x1E: parseWMTemp2Message(); handled = true; break;
	    }
	    break;
	case addressMM10:
	    /* MM10 message */
	    switch (m_type) {
		case 0xAB: parseMMTempMessage(); handled = true; break;
	    }
	    break;
    }

    if (!handled) {
	DebugStream& dataDebug = Options::dataDebug();
	if (dataDebug) {
	    dataDebug << "DATA: Unhandled message received";
	    dataDebug << "(source " << BYTEFORMAT_HEX m_source << ", type ";
	    dataDebug << BYTEFORMAT_HEX m_type << ")." << std::endl;
	}
    }
}

void
EmsMessage::printNumberAndAddToDb(size_t offset, size_t size, int divider,
				  const char *name, const char *unit,
				  Database::NumericSensors sensor)
{
    int value = 0;
    float floatVal;

    for (size_t i = offset; i < offset + size; i++) {
	value = (value << 8) | m_data[i];
    }

    /* treat values with highest bit set as negative
     * e.g. size = 2, value = 0xfffe -> real value -2
     */
    if (m_data[offset] & 0x80) {
	value = value - (1 << (size * 8));
    }

    floatVal = value;
    if (divider > 1) {
	floatVal /= divider;
    }

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: " << name << " = " << floatVal << " " << unit << std::endl;
    }
    if (m_db && sensor != Database::NumericSensorLast) {
	m_db->addSensorValue(sensor, floatVal);
    }
}

void
EmsMessage::printBoolAndAddToDb(int byte, int bit, const char *name,
				Database::BooleanSensors sensor)
{
    bool flagSet = m_data[byte] & (1 << bit);

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: " << name << " = "
			     << (flagSet ? "AN" : "AUS") << std::endl;
    }

    if (m_db && sensor != Database::BooleanSensorLast) {
	m_db->addSensorValue(sensor, flagSet);
    }
}

void
EmsMessage::printStateAndAddToDb(const std::string& value, const char *name,
				 Database::StateSensors sensor)
{
    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: " << name << " = " << value << std::endl;
    }

    if (m_db && sensor != Database::StateSensorLast) {
	m_db->addSensorValue(sensor, value);
    }
}

void
EmsMessage::parseUBAMonitorFastMessage()
{
    std::ostringstream ss;

    RETURN_ON_SIZE_MISMATCH(26, "Monitor Fast");

    printNumberAndAddToDb(1, 1, 1, "Kessel-Solltemperatur",
			  "°C", Database::SensorKesselSollTemp);
    printNumberAndAddToDb(2, 2, 10, "Kessel-Isttemperatur",
			  "°C", Database::SensorKesselIstTemp);
    printNumberAndAddToDb(12, 2, 10, "Warmwassertemperatur",
			  "°C", Database::NumericSensorLast);
    printNumberAndAddToDb(14, 2, 10, "Rücklauftemperatur",
			  "°C", Database::SensorRuecklaufTemp);
    printNumberAndAddToDb(4, 1, 1, "Max. Leistung", "%",
			  Database::SensorMaxLeistung /* FIXME: remove */);
    printNumberAndAddToDb(5, 1, 1, "Mom. Leistung", "%",
			  Database::SensorMomLeistung);
    printNumberAndAddToDb(16, 2, 10, "Flammenstrom", "µA",
			  Database::SensorFlammenstrom);
    printNumberAndAddToDb(18, 1, 10, "Systemdruck", "bar",
			  Database::SensorSystemdruck);

    ss << m_data[19] << m_data[20];
    printStateAndAddToDb(ss.str(), "Servicecode", Database::SensorServiceCode);
    ss.str(std::string());
    ss << std::dec << (m_data[21] << 8 | m_data[22]);
    printStateAndAddToDb(ss.str(), "Fehlercode", Database::SensorFehlerCode);

    printBoolAndAddToDb(8, 0, "Flamme", Database::SensorFlamme);
    printBoolAndAddToDb(8, 2, "Brenner/Abluft", Database::SensorBrenner);
    printBoolAndAddToDb(8, 3, "Zündung", Database::SensorZuendung);
    printBoolAndAddToDb(8, 5, "Kessel-Pumpe", Database::SensorKesselPumpe);
    printBoolAndAddToDb(8, 6, "3-Wege-Ventil auf WW", Database::Sensor3WegeVentil);
    printBoolAndAddToDb(8, 7, "Zirkulation", Database::SensorZirkulation);
}

void
EmsMessage::parseUBAMonitorSlowMessage()
{
    RETURN_ON_SIZE_MISMATCH(26, "Monitor Slow");

    printNumberAndAddToDb(1, 2, 10, "Außentemperatur", "°C",
			  Database::SensorAussenTemp);
    printNumberAndAddToDb(3, 2, 10, "Kesseltemperatur", "°C",
			  Database::NumericSensorLast);
    printNumberAndAddToDb(5, 2, 10, "Abgastemperatur", "°C",
			  Database::NumericSensorLast);
    printNumberAndAddToDb(11, 3, 1, "Brennerstarts", "",
			  Database::SensorBrennerstarts);
    printNumberAndAddToDb(14, 3, 1, "Betriebszeit total", "min",
			  Database::SensorBetriebszeit);
    printNumberAndAddToDb(20, 3, 1, "Betriebszeit Heizen", "min",
			  Database::SensorHeizZeit);
    printNumberAndAddToDb(23, 3, 1, "Betriebszeit 2", "min",
			  Database::NumericSensorLast);
}

void
EmsMessage::parseUBAMonitorWWMessage()
{
    DebugStream& debug = Options::dataDebug();

    RETURN_ON_SIZE_MISMATCH(17, "Monitor WW");

    printNumberAndAddToDb(1, 1, 1, "Warmwasser-Solltemperatur", "°C",
			  Database::SensorWarmwasserSollTemp);
    printNumberAndAddToDb(2, 2, 10, "Warmwasser-Isttemperatur (Messstelle 1)", "°C",
			  Database::SensorWarmwasserIstTemp);
    printNumberAndAddToDb(11, 3, 1, "Warmwasserbereitungszeit", "min",
			  Database::SensorWarmwasserbereitungsZeit);
    printNumberAndAddToDb(14, 3, 1, "Warmwasserbereitungen", "",
			  Database::SensorWarmwasserBereitungen);
    printNumberAndAddToDb(4, 2, 10, "Warmwasser-Isttemperatur (Messstelle 2)", "°C",
			  Database::NumericSensorLast);

    printBoolAndAddToDb(6, 0, "Tagbetrieb", Database::SensorWWTagbetrieb);
    printBoolAndAddToDb(6, 1, "Einmalladung", Database::BooleanSensorLast);
    printBoolAndAddToDb(6, 2, "Thermische Desinfektion", Database::BooleanSensorLast);
    printBoolAndAddToDb(6, 3, "Warmwasserbereitung", Database::SensorWarmwasserBereitung);
    printBoolAndAddToDb(6, 4, "Warmwassernachladung", Database::BooleanSensorLast);
    printBoolAndAddToDb(6, 5, "Warmwassertemp OK", Database::SensorWarmwasserTempOK);

    printBoolAndAddToDb(8, 2, "Zirkulation", Database::SensorZirkulation);

    if (debug) {
	debug << "DATA: Art des Warmwassersystems: ";
	if (m_data[9] & (1 << 0)) {
	    debug << "keins ";
	}
	if (m_data[9] & (1 << 1)) {
	    debug << "Durchlauferhitzer ";
	}
	if (m_data[9] & (1 << 2)) {
	    debug << "kleiner Speicher ";
	}
	if (m_data[9] & (1 << 3)) {
	    debug << "großer Speicher ";
	}
	if (m_data[9] & (1 << 4)) {
	    debug << "Speicherladesystem ";
	}
	debug << std::endl;
    }
}

void
EmsMessage::parseUBAUnknown1Message()
{
    RETURN_ON_SIZE_MISMATCH(4, "Unknown1");

    printNumberAndAddToDb(0, 2, 1, "Unbekannte Temperatur1", "°C",
			  Database::NumericSensorLast);
    printNumberAndAddToDb(2, 2, 1, "Unbekannter Zähler", "",
			  Database::NumericSensorLast);
}

void
EmsMessage::parseRCTimeMessage()
{
    RETURN_ON_SIZE_MISMATCH(9, "RC Time");

    DebugStream& debug = Options::dataDebug();
    if (debug) {
	debug << "DATA: Datum = " << BYTEFORMAT_DEC m_data[4] << ".";
	debug << BYTEFORMAT_DEC m_data[2] << ".";
	debug << BYTEFORMAT_DEC (2000 + m_data[1]) << std::endl;
	debug << "DATA: Zeit = " << BYTEFORMAT_DEC m_data[3] << ":";
	debug << BYTEFORMAT_DEC m_data[5] << ":";
	debug << BYTEFORMAT_DEC m_data[6] << std::endl;
	debug << "DATA: Wochentag = ";
	switch (m_data[7]) {
	    case 0: debug << "Montag"; break;
	    case 1: debug << "Dienstag"; break;
	    case 2: debug << "Mittwoch"; break;
	    case 3: debug << "Donnerstag"; break;
	    case 4: debug << "Freitag"; break;
	    case 5: debug << "Samstag"; break;
	    case 6: debug << "Sonntag"; break;
	    default: debug << "???" << BYTEFORMAT_DEC m_data[7];
	}
	debug << std::endl;

	debug << "DATA: Konfiguration = ";
	if (m_data[8] & (1 << 0)) {
	    debug << "Sommerzeit ";
	}
	if (m_data[8] & (1 << 1)) {
	    debug << "Funkuhr ";
	}
	if (m_data[8] & (1 << 4)) {
	    debug << "Uhr läuft ";
	}
	debug << std::endl;
    }
}

void
EmsMessage::parseRCOutdoorTempMessage()
{
    printNumberAndAddToDb(1, 1, 1, "Gedämpfte Außentemperatur", "°C",
			  Database::SensorGedaempfteAussenTemp);
}

void
EmsMessage::parseRCHKMonitorMessage(const char *name,
				    Database::NumericSensors vorlaufSollSensor,
				    Database::BooleanSensors automatikSensor,
				    Database::BooleanSensors tagSensor,
				    Database::BooleanSensors ferienSensor,
				    Database::BooleanSensors partySensor)
{
    std::string text;
    DebugStream& debug = Options::dataDebug();

    text = "RC ";
    text += name;
    text += " Monitor";

    RETURN_ON_SIZE_MISMATCH(16, text);

    printNumberAndAddToDb(3, 1, 2, "Raum-Solltemperatur", "°C",
			  Database::SensorRaumSollTemp);
    printNumberAndAddToDb(4, 2, 10, "Raum-Isttemperatur", "°C",
			  Database::SensorRaumIstTemp);

    if (debug) {
	debug << "DATA: Kennlinie " << name << " "
	      << "10°C -> " << BYTEFORMAT_DEC m_data[8]
	      << "°C / 0°C -> " << BYTEFORMAT_DEC m_data[9]
	      << "°C / -10°C -> " << BYTEFORMAT_DEC m_data[10]
	      << "°C" << std::endl;

	debug << "DATA: Einschaltoptimierungszeit "
	      << BYTEFORMAT_DEC m_data[6] << " min" << std::endl;
	debug << "DATA: Ausschaltoptimierungszeit "
	      << BYTEFORMAT_DEC m_data[7] << " min" << std::endl;
    }

    text = "Vorlauf ";
    text += name;
    text += " Solltemperatur";
    printNumberAndAddToDb(15, 1, 1, text.c_str(), "°C", vorlaufSollSensor);

    if (m_data[16] & (1 << 0)) {
	if (debug) {
	    debug << "DATA: " << name << " Keine Raumtemperatur vorhanden" << std::endl;
	}
    } else {
	text = name;
	text += " Raumtemperatur-Änderungsgeschwindigkeit";
	printNumberAndAddToDb(11, 2, 100, text.c_str(),
			      "K/min", Database::NumericSensorLast);
    }

    printBoolAndAddToDb(1, 2, "Automatikbetrieb", automatikSensor);
    printBoolAndAddToDb(1, 0, "Ausschaltoptimierung", Database::BooleanSensorLast);
    printBoolAndAddToDb(1, 1, "Einschaltoptimierung", Database::BooleanSensorLast);
    printBoolAndAddToDb(1, 3, "Warmwasservorrang", Database::SensorWWVorrang);
    printBoolAndAddToDb(1, 4, "Estrichtrocknung", Database::BooleanSensorLast);
    printBoolAndAddToDb(1, 5, "Ferienbetrieb", ferienSensor);
    printBoolAndAddToDb(1, 6, "Frostschutz", Database::BooleanSensorLast);
    printBoolAndAddToDb(1, 7, "Manueller Betrieb", Database::BooleanSensorLast);
    printBoolAndAddToDb(2, 0, "Sommerbetrieb", Database::SensorSommerbetrieb);
    printBoolAndAddToDb(2, 1, "Tagbetrieb", tagSensor);
    printBoolAndAddToDb(2, 7, "Partybetrieb", partySensor);

    text = "Schaltuhr ";
    text += name;
    printBoolAndAddToDb(14, 4, text.c_str(), Database::BooleanSensorLast);
}

void
EmsMessage::parseWMTemp1Message()
{
    RETURN_ON_SIZE_MISMATCH(6, "WM1 Temp");

    printNumberAndAddToDb(1, 2, 10, "Vorlauf HK1 Isttemperatur", "°C",
			  Database::SensorVorlaufHK1IstTemp);

    /* Byte 3 = 0 -> Pumpe aus, 100 = 0x64 -> Pumpe an */
    printBoolAndAddToDb(3, 2, "HK1 Pumpe", Database::SensorHK1Pumpe);
}

void
EmsMessage::parseWMTemp2Message()
{
    RETURN_ON_SIZE_MISMATCH(3, "WM2 Temp");

    printNumberAndAddToDb(1, 2, 10, "Vorlauf HK1 Isttemperatur", "°C",
			  Database::SensorVorlaufHK1IstTemp);
}

void
EmsMessage::parseMMTempMessage()
{
    RETURN_ON_SIZE_MISMATCH(8, "MM Temp");

    printNumberAndAddToDb(1, 1, 1, "Vorlauf HK2 Solltemperatur", "°C",
			  Database::SensorVorlaufHK2SollTemp);
    printNumberAndAddToDb(2, 2, 10, "Vorlauf HK2 Isttemperatur", "°C",
			  Database::SensorVorlaufHK2IstTemp);
    printNumberAndAddToDb(5, 1, 1, "Mischersteuerung", "",
			  Database::SensorMischersteuerung);

    /* Byte 4 = 0 -> Pumpe aus, 100 = 0x64 -> Pumpe an */
    printBoolAndAddToDb(4, 2, "HK2 Pumpe", Database::SensorHK2Pumpe);

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: MM10 Flags "
			     << BYTEFORMAT_HEX m_data[7] << std::endl;
    }
}

