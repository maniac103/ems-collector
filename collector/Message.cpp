#include <iostream>
#include <iomanip>
#include <cassert>
#include "Message.h"
#include "Options.h"

#define BYTEFORMAT_HEX \
    "0x" << std::setbase(16) << std::setw(2) << std::setfill('0') << (unsigned int)
#define BYTEFORMAT_DEC \
    std::dec << (unsigned int)

#define RETURN_ON_SIZE_MISMATCH(expected,text)                 \
    if (m_buffer.size() != expected) {                         \
	std::cerr << text << " size mismatch (";               \
	std::cerr << std::dec << m_buffer.size() << " vs. ";   \
	std::cerr << expected << ")" << std::endl;             \
	return;                                                \
    }

void
Message::parse()
{
    unsigned char source, dest, type;
    bool handled = false;
    DebugStream& debug = Options::messageDebug();

    assert(m_fill == m_buffer.size());

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
	debug << "]: ";
	for (size_t i = 0; i < m_buffer.size(); i++) {
	    debug << " " << BYTEFORMAT_HEX m_buffer[i];
	}
	debug << std::endl;
    }

    source = m_buffer[0];
    dest = m_buffer[1];
    type = m_buffer[2];

    if (dest & 0x80) {
	/* if highest bit of dest is set, it's a polling request -> ignore */
	return;
    }

    /* strip source, dest, type */
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + 3);

    m_db.startTransaction();

    switch (source) {
	case 0x08:
	    /* UBA message */
	    switch (type) {
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
		case 0x18: parseUBAStatus1Message(); handled = true; break;
		case 0x19: parseUBAStatus2Message(); handled = true; break;
		case 0x1c:
		    /* unknown message with varying length
		     * 0x8 0x10 0x1c 0x0 0x8a 0x4 0x13 0x1c 0x1d 0x0 0x0 0x0
		     * 0x8 0x10 0x1c 0x8
		     */
		    break;
		case 0x34: parseUBAStatus3Message(); handled = true; break;
	    }
	    break;
	case 0x09:
	    /* BC10 message */
	    switch (type) {
		case 0x29:
		    /* yet unknown: 0x9 0x10 0x29 0x0 0x6b */
		    break;
	    }
	    break;
	case 0x10:
	    /* RC message */
	    switch (type) {
		case 0x06: parseRCTimeMessage(); handled = true; break;
		case 0x1A: /* command for UBA3 */ handled = true; break;
		case 0x35: /* command for UBA3 */ handled = true; break;
		case 0x3E: parseRCHK1StatusMessage(); handled = true; break;
		case 0x48: parseRCHK2StatusMessage(); handled = true; break;
		case 0x9D: /* command for WM10 */ handled = true; break;
		case 0xA2: /* unknown, 11 zeros */ break;
		case 0xA3: parseRCOutdoorTempMessage(); handled = true; break;
		case 0xAC: /* command for MM10 */ handled = true; break;
	    }
	case 0x11:
	    /* WM10 message */
	    switch (type) {
		case 0x9C: parseWMTemp1Message(); handled = true; break;
		case 0x1E: parseWMTemp2Message(); handled = true; break;
	    }
	    break;
	case 0x21:
	    /* MM10 message */
	    switch (type) {
		case 0xAB: parseMMTempMessage(); handled = true; break;
	    }
	    break;
    }

    m_db.finishTransaction(handled);

    if (!handled) {
	DebugStream& dataDebug = Options::dataDebug();
	if (dataDebug) {
	    dataDebug << "DATA: Unhandled message received";
	    dataDebug << "(source " << BYTEFORMAT_HEX source << ", type ";
	    dataDebug << BYTEFORMAT_HEX type << ")." << std::endl;
	}
    }
}

void
Message::printNumberAndAddToDb(size_t offset, size_t size, int divider,
			       const char *name, const char *unit,
			       Database::NumericSensors sensor)
{
    int value = 0;
    float floatVal;

    for (size_t i = offset; i < offset + size; i++) {
	value = (value << 8) | m_buffer[i];
    }

    /* treat values with highest bit set as negative
     * e.g. size = 2, value = 0xfffe -> real value -2
     */
    if (m_buffer[offset] & 0x80) {
	value = value - (1 << (size * 8));
    }

    floatVal = value;
    if (divider > 1) {
	floatVal /= divider;
    }

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: " << name << " = " << floatVal << " " << unit << std::endl;
    }
    if (sensor != Database::NumericSensorLast) {
	m_db.addSensorValue(sensor, floatVal);
    }
}

void
Message::printBoolAndAddToDb(int byte, int bit, const char *name,
			     Database::BooleanSensors sensor)
{
    bool flagSet = m_buffer[byte] & (1 << bit);

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: " << name << " = "
			     << (flagSet ? "AN" : "AUS") << std::endl;
    }

    if (sensor != Database::BooleanSensorLast) {
	m_db.addSensorValue(sensor, flagSet);
    }
}

void
Message::parseUBAStatus1Message()
{
    RETURN_ON_SIZE_MISMATCH(26, "Status1");

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

    DebugStream& debug = Options::dataDebug();
    if (debug) {
	debug << "DATA: Servicecode = " << m_buffer[19] << m_buffer[20] << std::endl;
	debug << "DATA: Fehlercode = " << BYTEFORMAT_DEC m_buffer[22] << std::endl;
	debug << "DATA: Betriebsart = ";
	switch (m_buffer[24]) {
	    case 0: debug << "Auto"; break;
	    case 1: debug << "Nacht"; break;
	    case 2: debug << "Tag"; break;
	    case 3: debug << "Warmwasser"; break;
	    default: debug << "??? " << BYTEFORMAT_DEC m_buffer[24]; break;
	}
	debug << std::endl;
    }

    printBoolAndAddToDb(8, 0, "Flamme", Database::SensorFlamme);
    printBoolAndAddToDb(8, 2, "Brenner/Abluft", Database::SensorBrenner);
    printBoolAndAddToDb(8, 3, "Zündung", Database::SensorZuendung);
    printBoolAndAddToDb(8, 5, "HK Pumpe", Database::SensorHKPumpe);
    printBoolAndAddToDb(8, 6, "HK/WW", Database::SensorHKWW);
    printBoolAndAddToDb(8, 7, "Zirkulation", Database::SensorZirkulation);
    printBoolAndAddToDb(7, 0, "3-Wege-Ventil Heizen", Database::Sensor3WegeHeizen);
    printBoolAndAddToDb(7, 4, "3-Wege-Ventil WW", Database::Sensor3WegeWW);
}

void
Message::parseUBAStatus2Message()
{
    RETURN_ON_SIZE_MISMATCH(26, "Status2");

    printNumberAndAddToDb(1, 2, 10, "Außentemperatur", "°C",
			  Database::SensorAussenTemp);
    printNumberAndAddToDb(3, 2, 10, "Kesseltemperatur", "°C",
			  Database::NumericSensorLast);
    printNumberAndAddToDb(11, 3, 1, "Brennerstarts", "",
			  Database::SensorBrennerstarts);
    printNumberAndAddToDb(14, 3, 60, "Betriebszeit total", "h",
			  Database::SensorBetriebszeit);
    printNumberAndAddToDb(20, 3, 60, "Betriebszeit 1", "h",
			  Database::NumericSensorLast);
    printNumberAndAddToDb(23, 3, 60, "Betriebszeit 2", "h",
			  Database::NumericSensorLast);
}

void
Message::parseUBAStatus3Message()
{
    RETURN_ON_SIZE_MISMATCH(17, "Status3");

    printNumberAndAddToDb(1, 1, 1, "Warmwasser-Solltemperatur", "°C",
			  Database::SensorWarmwasserSollTemp);
    printNumberAndAddToDb(2, 2, 10, "Warmwasser-Isttemperatur", "°C",
			  Database::SensorWarmwasserIstTemp);
    printNumberAndAddToDb(11, 3, 10 * 60, "Warmwasserbereitungszeit", "h",
			  Database::SensorWarmwasserbereitungsZeit);
    printNumberAndAddToDb(14, 3, 1, "Warmwasserbereitungen", "",
			  Database::SensorWarmwasserBereitungen);
    /* TODO */
    printNumberAndAddToDb(4, 2, 10, "Mom. Wassertemperatur", "°C",
			  Database::NumericSensorLast /* FIXME? */);

    printBoolAndAddToDb(6, 0, "Tagbetrieb", Database::SensorTagbetrieb);
    printBoolAndAddToDb(6, 3, "Warmwasserbereitung", Database::SensorWarmwasserBereitung);
    printBoolAndAddToDb(6, 5, "3-Wege-Ventil Heizen", Database::BooleanSensorLast);
}

void
Message::parseUBAUnknown1Message()
{
    RETURN_ON_SIZE_MISMATCH(4, "Unknown1");

    printNumberAndAddToDb(0, 2, 1, "Unbekannte Temperatur1", "°C",
			  Database::NumericSensorLast);
    printNumberAndAddToDb(2, 2, 1, "Unbekannter Zähler", "",
			  Database::NumericSensorLast);
}

void
Message::parseRCTimeMessage()
{
    RETURN_ON_SIZE_MISMATCH(9, "RC Time");

    DebugStream& debug = Options::dataDebug();
    if (debug) {
	debug << "DATA: Datum =  " << BYTEFORMAT_DEC m_buffer[4] << ".";
	debug << BYTEFORMAT_DEC m_buffer[2] << ".";
	debug << BYTEFORMAT_DEC (2000 + m_buffer[1]) << std::endl;
	debug << "DATA: Zeit = " << BYTEFORMAT_DEC m_buffer[3] << ":";
	debug << BYTEFORMAT_DEC m_buffer[5] << ":";
	debug << BYTEFORMAT_DEC m_buffer[6] << std::endl;
	debug << "DATA: Wochentag = ";
	switch (m_buffer[7]) {
	    case 0: debug << "Montag"; break;
	    case 1: debug << "Dienstag"; break;
	    case 2: debug << "Mittwoch"; break;
	    case 3: debug << "Donnerstag"; break;
	    case 4: debug << "Freitag"; break;
	    case 5: debug << "Samstag"; break;
	    case 6: debug << "Sonntag"; break;
	    default: debug << "???" << BYTEFORMAT_DEC m_buffer[7];
	}
	debug << std::endl;
    }
}

void
Message::parseRCOutdoorTempMessage()
{
    printNumberAndAddToDb(1, 1, 1, "Gedämpfte Außentemperatur", "°C",
			  Database::SensorGedaempfteAussenTemp);
}

void
Message::parseRCHK1StatusMessage()
{
    RETURN_ON_SIZE_MISMATCH(16, "RC HK1 Status");

    printNumberAndAddToDb(3, 1, 2, "Raum-Solltemperatur", "°C",
			  Database::SensorRaumSollTemp);
    printNumberAndAddToDb(4, 2, 10, "Raum-Isttemperatur", "°C",
			  Database::SensorRaumIstTemp);

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: Kennlinie HK1 "
			     << "10°C -> " << BYTEFORMAT_DEC m_buffer[8]
			     << "°C / 0°C -> " << BYTEFORMAT_DEC m_buffer[9]
			     << "°C / -10°C -> " << BYTEFORMAT_DEC m_buffer[10]
			     << "°C" << std::endl;
    }

    printNumberAndAddToDb(15, 1, 1, "Vorlauf HK1 Solltemperatur", "°C",
			  Database::SensorVorlaufHK1SollTemp);
    printBoolAndAddToDb(2, 1, "Tagbetrieb", Database::BooleanSensorLast);
    printBoolAndAddToDb(14, 4, "Pumpe HK1", Database::BooleanSensorLast);
}

void
Message::parseRCHK2StatusMessage()
{
    RETURN_ON_SIZE_MISMATCH(16, "RC HK2 Status");

    printNumberAndAddToDb(3, 1, 2, "Raum-Solltemperatur", "°C",
			  Database::SensorRaumSollTemp);
    printNumberAndAddToDb(4, 2, 10, "Raum-Isttemperatur", "°C",
			  Database::SensorRaumIstTemp);

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: Kennlinie HK2 "
			     << "10°C -> " << BYTEFORMAT_DEC m_buffer[8]
			     << "°C / 0°C -> " << BYTEFORMAT_DEC m_buffer[9]
			     << "°C / -10°C -> " << BYTEFORMAT_DEC m_buffer[10]
			     << "°C" << std::endl;
    }

    printNumberAndAddToDb(15, 1, 1, "Vorlauf HK2 Solltemperatur", "°C",
			  Database::SensorVorlaufHK2SollTemp);
    printBoolAndAddToDb(2, 1, "Tagbetrieb", Database::BooleanSensorLast);
    printBoolAndAddToDb(14, 4, "Pumpe HK2", Database::BooleanSensorLast);
}

void
Message::parseWMTemp1Message()
{
    RETURN_ON_SIZE_MISMATCH(6, "WM1 Temp");

    printNumberAndAddToDb(1, 2, 10, "Vorlauf HK1 Isttemperatur", "°C",
			  Database::SensorVorlaufHK1IstTemp);
}

void
Message::parseWMTemp2Message()
{
    RETURN_ON_SIZE_MISMATCH(3, "WM2 Temp");

    printNumberAndAddToDb(1, 2, 10, "Vorlauf HK1 Isttemperatur", "°C",
			  Database::SensorVorlaufHK1IstTemp);
}

void
Message::parseMMTempMessage()
{
    RETURN_ON_SIZE_MISMATCH(8, "MM Temp");

    printNumberAndAddToDb(1, 1, 1, "Vorlauf HK2 Solltemperatur", "°C",
			  Database::SensorVorlaufHK2SollTemp);
    printNumberAndAddToDb(2, 2, 10, "Vorlauf HK2 Isttemperatur", "°C",
			  Database::SensorVorlaufHK2IstTemp);
    printNumberAndAddToDb(5, 1, 1, "Mischersteuerung", "",
			  Database::SensorMischersteuerung);

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: MM10 Flags "
			     << BYTEFORMAT_HEX m_buffer[7] << std::endl;
    }
}

