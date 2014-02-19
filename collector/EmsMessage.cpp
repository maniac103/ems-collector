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

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include "EmsMessage.h"
#include "Options.h"

#define BYTEFORMAT_HEX \
    "0x" << std::setbase(16) << std::setw(2) << std::setfill('0') << (unsigned int)
#define BYTEFORMAT_DEC \
    std::dec << (unsigned int)

#define RETURN_ON_SIZE_MISMATCH(expected,text)                        \
    if (m_data.size() < expected) {                                   \
	std::cerr << text << " size mismatch (got ";                  \
	std::cerr << std::dec << m_data.size() << ", expected min. "; \
	std::cerr << expected << ")" << std::endl;                    \
	return;                                                       \
    }

EmsValue::EmsValue(Type type, SubType subType, const uint8_t *data, size_t len, int divider) :
    m_type(type),
    m_subType(subType),
    m_readingType(Numeric)
{
    int value = 0;
    for (size_t i = 0; i < len; i++) {
	value = (value << 8) | data[i];
    }

    /* treat values with highest bit set as negative
     * e.g. size = 2, value = 0xfffe -> real value -2
     */
    if (data[0] & 0x80) {
	value = value - (1 << (len * 8));
    }

    m_value = (double) value / (double) divider;
}

EmsValue::EmsValue(Type type, SubType subType, uint8_t value, uint8_t bit) :
    m_type(type),
    m_subType(subType),
    m_readingType(Boolean),
    m_value((value & (1 << bit)) != 0)
{
}

EmsValue::EmsValue(Type type, SubType subType, uint8_t low, uint8_t medium, uint8_t high) :
    m_type(type),
    m_subType(subType),
    m_readingType(Kennlinie),
    m_value(std::vector<uint8_t>({ low, medium, high }))
{
}

EmsValue::EmsValue(Type type, SubType subType, uint8_t value) :
    m_type(type),
    m_subType(subType),
    m_readingType(Enumeration),
    m_value(value)
{
}

EmsValue::EmsValue(Type type, SubType subType, const ErrorEntry& error) :
    m_type(type),
    m_subType(subType),
    m_readingType(Error),
    m_value(error)
{
}

EmsValue::EmsValue(Type type, SubType subType, const std::string& value) :
    m_type(type),
    m_subType(subType),
    m_readingType(Formatted),
    m_value(value)
{
}

EmsMessage::EmsMessage(ValueHandler& valueHandler, const std::vector<uint8_t>& data) :
    m_valueHandler(valueHandler),
    m_data(data)
{
    if (m_data.size() >= 4) {
	m_source = m_data[0];
	m_dest = m_data[1];
	m_type = m_data[2];
	m_offset = m_data[3];
	m_data.erase(m_data.begin(), m_data.begin() + 4);
    } else {
	m_source = 0;
	m_dest = 0;
	m_type = 0;
	m_offset = 0;
    }
}

EmsMessage::EmsMessage(uint8_t dest, uint8_t type, uint8_t offset,
		       const std::vector<uint8_t>& data,
		       bool expectResponse) :
    m_valueHandler(),
    m_data(data),
    m_source(EmsProto::addressPC),
    m_dest(dest | (expectResponse ? 0x80 : 0)),
    m_type(type),
    m_offset(offset)
{
}

std::vector<uint8_t>
EmsMessage::getSendData() const
{
    std::vector<uint8_t> data;

    /* own address omitted on send */
    data.push_back(m_dest);
    data.push_back(m_type);
    data.push_back(m_offset);
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
	debug << ", offset " << BYTEFORMAT_DEC m_offset;
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
	case EmsProto::addressUBA:
	    /* UBA message */
	    switch (m_type) {
		case 0x07:
		    /* yet unknown contents:
		     * 0x8 0x0 0x7 0x0 0x3 0x3 0x0 0x2 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0 0x0 */
		    break;
		case 0x10:
		case 0x11:
		    parseUBAErrorMessage();
		    handled = true;
		    break;
		case 0x14: parseUBATotalUptimeMessage(); handled = true; break;
		case 0x15: parseUBAMaintenanceSettingsMessage(); handled = true; break;
		case 0x16: parseUBAParametersMessage(); handled = true; break;
		case 0x18: parseUBAMonitorFastMessage(); handled = true; break;
		case 0x19: parseUBAMonitorSlowMessage(); handled = true; break;
		case 0x1c: parseUBAMaintenanceStatusMessage(); handled = true; break;
		case 0x33: parseUBAParameterWWMessage(); handled = true; break;
		case 0x34: parseUBAMonitorWWMessage(); handled = true; break;
	    }
	    break;
	case EmsProto::addressBC10:
	    /* BC10 message */
	    switch (m_type) {
		case 0x29:
		    /* yet unknown: 0x9 0x10 0x29 0x0 0x6b */
		    break;
	    }
	    break;
	case EmsProto::addressRC:
	    /* RC message */
	    switch (m_type) {
		case 0x06: parseRCTimeMessage(); handled = true; break;
		case 0x1A: /* command for UBA3 */ handled = true; break;
		case 0x35: /* command for UBA3 */ handled = true; break;
		case 0x37: parseRCWWOpmodeMessage(); handled = true; break;
		case 0x3D:
		    parseRCHKOpmodeMessage("HK1", EmsValue::HK1);
		    handled = true;
		    break;
		case 0x47:
		    parseRCHKOpmodeMessage("HK2", EmsValue::HK2);
		    handled = true;
		    break;
		case 0x51:
		    parseRCHKOpmodeMessage("HK3", EmsValue::HK3);
		    handled = true;
		    break;
		case 0x5B:
		    parseRCHKOpmodeMessage("HK4", EmsValue::HK4);
		    handled = true;
		    break;
		case 0x3E:
		    parseRCHKMonitorMessage("HK1", EmsValue::HK1);
		    handled = true;
		    break;
		case 0x48:
		    parseRCHKMonitorMessage("HK2", EmsValue::HK2);
		    handled = true;
		    break;
		case 0x52:
		    parseRCHKMonitorMessage("HK3", EmsValue::HK3);
		    handled = true;
		    break;
		case 0x5C:
		    parseRCHKMonitorMessage("HK4", EmsValue::HK4);
		    handled = true;
		    break;
		case 0x3F:
		    parseRCHKScheduleMessage("HK1", EmsValue::HK1);
		    handled = true;
		    break;
		case 0x9D: /* command for WM10 */ handled = true; break;
		case 0xA2: /* unknown, 11 zeros */ break;
		case 0xA3: parseRCOutdoorTempMessage(); handled = true; break;
		case 0xA5: parseRCSystemParameterMessage(); handled = true; break;
		case 0xAC: /* command for MM10 */ handled = true; break;
	    }
	case EmsProto::addressWM10:
	    /* WM10 message */
	    switch (m_type) {
		case 0x9C: parseWMTemp1Message(); handled = true; break;
		case 0x1E: parseWMTemp2Message(); handled = true; break;
	    }
	    break;
	case EmsProto::addressMM10:
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
EmsMessage::parseEnum(size_t offset, 
		      EmsValue::Type type, EmsValue::SubType subtype)
{
    if (m_valueHandler && canAccess(offset, 1)) {
	EmsValue value(type, subtype, m_data[offset]);
	m_valueHandler(value);
    }
}

void
EmsMessage::parseNumeric(size_t offset, size_t size, int divider,
			 EmsValue::Type type, EmsValue::SubType subtype)
{
    if (m_valueHandler && canAccess(offset, size)) {
	EmsValue value(type, subtype, &m_data.at(offset - m_offset), size, divider);
	m_valueHandler(value);
    }
}

void
EmsMessage::parseBool(size_t offset, uint8_t bit,
		      EmsValue::Type type, EmsValue::SubType subtype)
{
    if (m_valueHandler && canAccess(offset, 1)) {
	EmsValue value(type, subtype, m_data.at(offset - m_offset), bit);
	m_valueHandler(value);
    }
}

void
EmsMessage::parseUBAMonitorFastMessage()
{
    RETURN_ON_SIZE_MISMATCH(25, "Monitor Fast");

    parseNumeric(0, 1, 1, EmsValue::SollTemp, EmsValue::Kessel);
    parseNumeric(1, 2, 10, EmsValue::IstTemp, EmsValue::Kessel);
    parseNumeric(11, 2, 10, EmsValue::IstTemp, EmsValue::WW);
    parseNumeric(13, 2, 10, EmsValue::IstTemp, EmsValue::Ruecklauf);
    parseNumeric(3, 1, 1, EmsValue::SollModulation, EmsValue::Brenner);
    parseNumeric(4, 1, 1, EmsValue::IstModulation, EmsValue::Brenner);
    parseNumeric(15, 2, 10, EmsValue::Flammenstrom, EmsValue::None);
    parseNumeric(17, 1, 10, EmsValue::Systemdruck, EmsValue::None);

    if (m_valueHandler) {
	if (canAccess(18, 2)) {
	    std::ostringstream ss;
	    ss << m_data[18] << m_data[19];
	    m_valueHandler(EmsValue(EmsValue::ServiceCode, EmsValue::None, ss.str()));
	}
	if (canAccess(20, 2)) {
	    std::ostringstream ss;
	    ss << std::dec << (m_data[20] << 8 | m_data[21]);
	    m_valueHandler(EmsValue(EmsValue::FehlerCode, EmsValue::None, ss.str()));
	}
    }

    parseBool(7, 0, EmsValue::FlammeAktiv, EmsValue::None);
    parseBool(7, 2, EmsValue::BrennerAktiv, EmsValue::None);
    parseBool(7, 3, EmsValue::ZuendungAktiv, EmsValue::None);
    parseBool(7, 5, EmsValue::PumpeAktiv, EmsValue::None);
    parseBool(7, 6, EmsValue::DreiWegeVentilAufWW, EmsValue::None);
    parseBool(7, 7, EmsValue::ZirkulationAktiv, EmsValue::None);
}

void
EmsMessage::parseUBATotalUptimeMessage()
{
    RETURN_ON_SIZE_MISMATCH(3, "Totaluptime");

    parseNumeric(0, 3, 1, EmsValue::BetriebsZeit, EmsValue::None);
}

void
EmsMessage::parseUBAMaintenanceSettingsMessage()
{
    RETURN_ON_SIZE_MISMATCH(5, "Maintenance Settings");

    parseEnum(0,EmsValue::Wartungsmeldungen, EmsValue::Kessel);
    parseNumeric(1, 1, 1, EmsValue::HektoStundenVorWartung, EmsValue::Kessel);
    parseNumeric(2, 1, 1, EmsValue::WartungsterminTag, EmsValue::Kessel);
    parseNumeric(3, 1, 1, EmsValue::WartungsterminMonat, EmsValue::Kessel);
    parseNumeric(4, 1, 1, EmsValue::WartungsterminJahr, EmsValue::Kessel);

}

void
EmsMessage::parseUBAMaintenanceStatusMessage()
{
    parseEnum(5, EmsValue::WartungFaellig, EmsValue::Kessel);
}

void
EmsMessage::parseUBAMonitorSlowMessage()
{
    RETURN_ON_SIZE_MISMATCH(25, "Monitor Slow");

    parseNumeric(0, 2, 10, EmsValue::IstTemp, EmsValue::Aussen);
    parseNumeric(2, 2, 10, EmsValue::IstTemp, EmsValue::Waermetauscher);
    parseNumeric(4, 2, 10, EmsValue::IstTemp, EmsValue::Abgas);
    parseNumeric(9, 1, 1, EmsValue::IstModulation, EmsValue::KesselPumpe);
    parseNumeric(10, 3, 1, EmsValue::Brennerstarts, EmsValue::Kessel);
    parseNumeric(13, 3, 1, EmsValue::BetriebsZeit, EmsValue::Kessel);
    parseNumeric(19, 3, 1, EmsValue::HeizZeit, EmsValue::Kessel);
}

void
EmsMessage::parseUBAMonitorWWMessage()
{
    RETURN_ON_SIZE_MISMATCH(16, "Monitor WW");

    parseNumeric(0, 1, 1, EmsValue::SollTemp, EmsValue::WW);
    parseNumeric(1, 2, 10, EmsValue::IstTemp, EmsValue::WW);
    parseNumeric(10, 3, 1, EmsValue::WarmwasserbereitungsZeit, EmsValue::None);
    parseNumeric(13, 3, 1, EmsValue::WarmwasserBereitungen, EmsValue::None);

    parseBool(5, 0, EmsValue::Tagbetrieb, EmsValue::WW);
    parseBool(5, 1, EmsValue::EinmalLadungAktiv, EmsValue::WW);
    parseBool(5, 2, EmsValue::DesinfektionAktiv, EmsValue::WW);
    parseBool(5, 3, EmsValue::WarmwasserBereitung, EmsValue::None);
    parseBool(5, 4, EmsValue::NachladungAktiv, EmsValue::WW);
    parseBool(5, 5, EmsValue::WarmwasserTempOK, EmsValue::None);
    parseBool(7, 0, EmsValue::Tagbetrieb, EmsValue::Zirkulation);
    parseBool(7, 2, EmsValue::ZirkulationAktiv, EmsValue::None);

    parseEnum(8, EmsValue::WWSystemType, EmsValue::None);
}

void
EmsMessage::parseUBAParameterWWMessage()
{
    RETURN_ON_SIZE_MISMATCH(9, "UBA Parameter WW");

    parseBool(1, 0, EmsValue::KesselSchalter, EmsValue::WW);
    parseNumeric(2, 1, 1, EmsValue::SetTemp, EmsValue::WW);
    parseNumeric(8, 1, 1, EmsValue::DesinfektionsTemp, EmsValue::WW);
    parseEnum(7,EmsValue::Schaltpunkte, EmsValue::Zirkulation);
}

void
EmsMessage::parseUBAErrorMessage()
{
    if (m_data.size() % sizeof(EmsProto::ErrorRecord) != 0) {
	std::cerr << "UBA error size mismatch (got " << m_data.size();
	std::cerr << ", expected a multiple of " << sizeof(EmsProto::ErrorRecord) << ")" << std::endl;
	return;
    }

    if (m_offset % sizeof(EmsProto::ErrorRecord) != 0) {
	std::cerr << "Unexpected offset (got " << m_offset;
	std::cerr << ", expected a multiple of " << sizeof(EmsProto::ErrorRecord);
	std::cerr << ")" << std::endl;
	return;
    }

    if (m_valueHandler) {
	size_t count = m_data.size() / sizeof(EmsProto::ErrorRecord);
	for (size_t i = 0; i < count; i++) {
	    EmsProto::ErrorRecord *record = (EmsProto::ErrorRecord *) &m_data.at(i * sizeof(EmsProto::ErrorRecord));
	    unsigned int index = (m_offset / sizeof(EmsProto::ErrorRecord)) + i;
	    EmsValue::ErrorEntry entry = { m_type, index, *record };

	    m_valueHandler(EmsValue(EmsValue::Fehler, EmsValue::None, entry));
	}
    }
}

void
EmsMessage::parseUBAParametersMessage()
{
    RETURN_ON_SIZE_MISMATCH(12, "UBA Parameters");

    parseBool(0, 1, EmsValue::KesselSchalter, EmsValue::Kessel);
    parseNumeric(1, 1, 1, EmsValue::SetTemp, EmsValue::Kessel);
    parseNumeric(2, 1, 1, EmsValue::MaxModulation, EmsValue::Brenner);
    parseNumeric(3, 1, 1, EmsValue::MinModulation, EmsValue::Brenner);
    parseNumeric(4, 1, 1, EmsValue::EinschaltHysterese, EmsValue::Kessel);
    parseNumeric(5, 1, 1, EmsValue::AusschaltHysterese, EmsValue::Kessel);
    parseNumeric(6, 1, 1, EmsValue::AntipendelZeit, EmsValue::None);
    parseNumeric(8, 1, 1, EmsValue::NachlaufZeit, EmsValue::KesselPumpe);
    parseNumeric(9, 1, 1, EmsValue::MaxModulation, EmsValue::KesselPumpe);
    parseNumeric(10, 1, 1, EmsValue::MinModulation, EmsValue::KesselPumpe);
}

void
EmsMessage::parseRCTimeMessage()
{
    RETURN_ON_SIZE_MISMATCH(8, "RC Time");

    DebugStream& debug = Options::dataDebug();
    if (debug) {
	debug << "DATA: Datum = " << BYTEFORMAT_DEC m_data[3] << ".";
	debug << BYTEFORMAT_DEC m_data[1] << ".";
	debug << BYTEFORMAT_DEC (2000 + m_data[0]) << std::endl;
	debug << "DATA: Zeit = " << BYTEFORMAT_DEC m_data[2] << ":";
	debug << BYTEFORMAT_DEC m_data[4] << ":";
	debug << BYTEFORMAT_DEC m_data[5] << std::endl;
	debug << "DATA: Wochentag = ";
	switch (m_data[6]) {
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
	if (m_data[7] & (1 << 0)) {
	    debug << "Sommerzeit ";
	}
	if (m_data[7] & (1 << 1)) {
	    debug << "Funkuhr ";
	}
	if (m_data[7] & (1 << 4)) {
	    debug << "Uhr läuft ";
	}
	debug << std::endl;
    }
}


void
EmsMessage::parseRCWWOpmodeMessage()
{ 
    RETURN_ON_SIZE_MISMATCH(10, "WW Opmode");

    parseBool(0, 1, EmsValue::EigenesProgrammAktiv, EmsValue::WW);
    parseBool(1, 1, EmsValue::EigenesProgrammAktiv, EmsValue::Zirkulation);

    parseEnum(2, EmsValue::Betriebsart, EmsValue::Zirkulation);
    parseEnum(3, EmsValue::Betriebsart, EmsValue::WW);

    parseBool(4, 1, EmsValue::Desinfektion, EmsValue::WW);
    parseEnum(5, EmsValue::DesinfektionTag, EmsValue::WW);
    parseNumeric(6, 1, 1, EmsValue::DesinfektionStunde, EmsValue::WW);
    parseNumeric(8, 1, 1, EmsValue::MaxTemp, EmsValue::WW);
    parseBool(9 ,1, EmsValue::EinmalLadungsLED, EmsValue::WW);
    
}

void
EmsMessage::parseRCSystemParameterMessage()
{
  // ToDo: Implement
}

void
EmsMessage::parseRCHKOpmodeMessage(const char *name, EmsValue::SubType subtype)
{
    std::string text;

    text = "RC ";
    text += name;
    text += " OpMode";

    // RETURN_ON_SIZE_MISMATCH(15, text);
    // ToDo: Implement

}

void
EmsMessage::parseRCHKScheduleMessage(const char *name, EmsValue::SubType subtype)
{
    std::string text;

    text = "RC ";
    text += name;
    text += " Schedule";

    // RETURN_ON_SIZE_MISMATCH(15, text);
    // ToDo: Implement

}

void
EmsMessage::parseRCOutdoorTempMessage()
{
    parseNumeric(0, 1, 1, EmsValue::GedaempfteTemp, EmsValue::Aussen);
}

void
EmsMessage::parseRCHKMonitorMessage(const char *name, EmsValue::SubType subtype)
{
    std::string text;

    text = "RC ";
    text += name;
    text += " Monitor";

    RETURN_ON_SIZE_MISMATCH(15, text);

    parseNumeric(2, 1, 2, EmsValue::SollTemp, EmsValue::Raum);
    parseNumeric(3, 2, 10, EmsValue::IstTemp, EmsValue::Raum);

    if (m_valueHandler && canAccess(7, 3)) {
	EmsValue value(EmsValue::HKKennlinie, subtype, m_data[7], m_data[8], m_data[9]);
	m_valueHandler(value);
    }

    parseNumeric(14, 1, 1, EmsValue::SollTemp, subtype);
    parseNumeric(5, 1, 1, EmsValue::EinschaltoptimierungsZeit, subtype);
    parseNumeric(6, 1, 1, EmsValue::AusschaltoptimierungsZeit, subtype);

    if (canAccess(15, 1) && (m_data[15] & 1) == 0) {
	parseNumeric(10, 2, 100, EmsValue::TemperaturAenderung, EmsValue::Raum);
    }

    parseBool(0, 2, EmsValue::Automatikbetrieb, subtype);
    parseBool(0, 0, EmsValue::Ausschaltoptimierung, subtype);
    parseBool(0, 1, EmsValue::Einschaltoptimierung, subtype);
    parseBool(0, 3, EmsValue::WWVorrang, subtype);
    parseBool(0, 4, EmsValue::Estrichtrocknung, subtype);
    parseBool(0, 5, EmsValue::Ferien, subtype);
    parseBool(0, 6, EmsValue::Frostschutz, subtype);
    parseBool(1, 0, EmsValue::Sommerbetrieb, subtype);
    parseBool(1, 1, EmsValue::Tagbetrieb, subtype);
    parseBool(1, 7, EmsValue::Party, subtype);
    parseBool(13, 4, EmsValue::SchaltuhrEin, subtype);
}

void
EmsMessage::parseWMTemp1Message()
{
    RETURN_ON_SIZE_MISMATCH(5, "WM1 Temp");

    parseNumeric(0, 2, 10, EmsValue::IstTemp, EmsValue::HK1);

    /* Byte 2 = 0 -> Pumpe aus, 100 = 0x64 -> Pumpe an */
    parseBool(2, 2, EmsValue::PumpeAktiv, EmsValue::HK1);
}

void
EmsMessage::parseWMTemp2Message()
{
    RETURN_ON_SIZE_MISMATCH(2, "WM2 Temp");

    parseNumeric(0, 2, 10, EmsValue::IstTemp, EmsValue::HK1);
}

void
EmsMessage::parseMMTempMessage()
{
    RETURN_ON_SIZE_MISMATCH(7, "MM Temp");

    parseNumeric(0, 1, 1, EmsValue::SollTemp, EmsValue::HK2);
    parseNumeric(1, 2, 10, EmsValue::IstTemp, EmsValue::HK2);
    parseNumeric(3, 1, 1, EmsValue::Mischersteuerung, EmsValue::None);

    /* Byte 3 = 0 -> Pumpe aus, 100 = 0x64 -> Pumpe an */
    parseBool(3, 2, EmsValue::PumpeAktiv, EmsValue::HK2);

    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: MM10 Flags "
			     << BYTEFORMAT_HEX m_data[6] << std::endl;
    }
}

