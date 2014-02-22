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
#include <boost/format.hpp>
#include "EmsMessage.h"
#include "Options.h"

EmsValue::EmsValue(Type type, SubType subType, const uint8_t *data, size_t len, int divider) :
    m_type(type),
    m_subType(subType),
    m_readingType(Numeric)
{
    int value = 0;
    for (size_t i = 0; i < len; i++) {
	value = (value << 8) | data[i];
    }

    if (divider == 0) {
	m_value = (unsigned int) value;
	m_readingType = Integer;
    } else {
	/* treat values with highest bit set as negative
	 * e.g. size = 2, value = 0xfffe -> real value -2
	 */
	if (data[0] & 0x80) {
	    value = value - (1 << (len * 8));
	}

	m_value = (float) value / (float) divider;
    }
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

EmsValue::EmsValue(Type type, SubType subType, const EmsProto::DateRecord& record) :
    m_type(type),
    m_subType(subType),
    m_readingType(SystemTime),
    m_value(record)
{
}

EmsValue::EmsValue(Type type, SubType subType, const EmsProto::SystemTimeRecord& record) :
    m_type(type),
    m_subType(subType),
    m_readingType(SystemTime),
    m_value(record)
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
	boost::format f("MESSAGE[%02d.%02d.%04d %02d:%02d:%02d]: "
			"source 0x%02x, dest 0x%02x, type 0x%02x, offset %d");
	f  % time.tm_mday % (time.tm_mon + 1) % (time.tm_year + 1900);
	f % time.tm_hour % time.tm_min % time.tm_sec;
	f % (unsigned int) m_source % (unsigned int) m_dest;
	f % (unsigned int) m_type % (unsigned int) m_offset;

	debug << f << ", data:";
	for (size_t i = 0; i < m_data.size(); i++) {
	    debug << " 0x" << std::hex << std::setw(2)
		  << std::setfill('0') << (unsigned int) m_data[i];
	}
	debug << std::endl;
    }

    if (!m_valueHandler) {
	/* kind of pointless to parse in that case */
	return;
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
		case 0x3D: parseRCHKOpmodeMessage(EmsValue::HK1); handled = true; break;
		case 0x3E: parseRCHKMonitorMessage(EmsValue::HK1); handled = true; break;
		case 0x3F: parseRCHKScheduleMessage(EmsValue::HK1); handled = true; break;
		case 0x47: parseRCHKOpmodeMessage(EmsValue::HK2); handled = true; break;
		case 0x48: parseRCHKMonitorMessage(EmsValue::HK2); handled = true; break;
		case 0x49: parseRCHKScheduleMessage(EmsValue::HK2); handled = true; break;
		case 0x51: parseRCHKOpmodeMessage(EmsValue::HK3); handled = true; break;
		case 0x52: parseRCHKMonitorMessage(EmsValue::HK3); handled = true; break;
		case 0x53: parseRCHKScheduleMessage(EmsValue::HK3); handled = true; break;
		case 0x5B: parseRCHKOpmodeMessage(EmsValue::HK4); handled = true; break;
		case 0x5C: parseRCHKMonitorMessage(EmsValue::HK4); handled = true; break;
		case 0x5D: parseRCHKScheduleMessage(EmsValue::HK4); handled = true; break;
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
	    dataDebug << boost::format("(source 0x%02x, type 0x%02x).")
		    % (unsigned int) m_source % (unsigned int) m_type;
	    dataDebug << std::endl;
	}
    }
}

void
EmsMessage::parseEnum(size_t offset, EmsValue::Type type, EmsValue::SubType subtype)
{
    if (canAccess(offset, 1)) {
	EmsValue value(type, subtype, m_data[offset - m_offset]);
	m_valueHandler(value);
    }
}

void
EmsMessage::parseInteger(size_t offset, size_t size,
			 EmsValue::Type type, EmsValue::SubType subtype)
{
    parseNumeric(offset, size, 0, type, subtype);
}

void
EmsMessage::parseNumeric(size_t offset, size_t size, int divider,
			 EmsValue::Type type, EmsValue::SubType subtype)
{
    if (canAccess(offset, size)) {
	EmsValue value(type, subtype, &m_data.at(offset - m_offset), size, divider);
	m_valueHandler(value);
    }
}

void
EmsMessage::parseBool(size_t offset, uint8_t bit,
		      EmsValue::Type type, EmsValue::SubType subtype)
{
    if (canAccess(offset, 1)) {
	EmsValue value(type, subtype, m_data.at(offset - m_offset), bit);
	m_valueHandler(value);
    }
}

void
EmsMessage::parseUBAMonitorFastMessage()
{
    parseNumeric(0, 1, 1, EmsValue::SollTemp, EmsValue::Kessel);
    parseNumeric(1, 2, 10, EmsValue::IstTemp, EmsValue::Kessel);
    parseNumeric(11, 2, 10, EmsValue::IstTemp, EmsValue::WW);
    parseNumeric(13, 2, 10, EmsValue::IstTemp, EmsValue::Ruecklauf);
    parseNumeric(3, 1, 1, EmsValue::SollModulation, EmsValue::Brenner);
    parseNumeric(4, 1, 1, EmsValue::IstModulation, EmsValue::Brenner);
    parseNumeric(15, 2, 10, EmsValue::Flammenstrom, EmsValue::None);
    parseNumeric(17, 1, 10, EmsValue::Systemdruck, EmsValue::None);

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
    parseInteger(0, 3, EmsValue::BetriebsZeit, EmsValue::None);
}

void
EmsMessage::parseUBAMaintenanceSettingsMessage()
{
    parseEnum(0,EmsValue::Wartungsmeldungen, EmsValue::Kessel);
    parseInteger(1, 1, EmsValue::HektoStundenVorWartung, EmsValue::Kessel);
    if (canAccess(2, sizeof(EmsProto::DateRecord))) {
	EmsProto::DateRecord *record = (EmsProto::DateRecord *) &m_data.at(2 - m_offset);
	m_valueHandler(EmsValue(EmsValue::Wartungstermin, EmsValue::Kessel, *record));
    }
}

void
EmsMessage::parseUBAMaintenanceStatusMessage()
{
    parseEnum(5, EmsValue::WartungFaellig, EmsValue::Kessel);
}

void
EmsMessage::parseUBAMonitorSlowMessage()
{
    parseNumeric(0, 2, 10, EmsValue::IstTemp, EmsValue::Aussen);
    parseNumeric(2, 2, 10, EmsValue::IstTemp, EmsValue::Waermetauscher);
    parseNumeric(4, 2, 10, EmsValue::IstTemp, EmsValue::Abgas);
    parseNumeric(9, 1, 1, EmsValue::IstModulation, EmsValue::KesselPumpe);
    parseInteger(10, 3, EmsValue::Brennerstarts, EmsValue::Kessel);
    parseInteger(13, 3, EmsValue::BetriebsZeit, EmsValue::Kessel);
    parseInteger(19, 3, EmsValue::HeizZeit, EmsValue::Kessel);
}

void
EmsMessage::parseUBAMonitorWWMessage()
{
    parseNumeric(0, 1, 1, EmsValue::SollTemp, EmsValue::WW);
    parseNumeric(1, 2, 10, EmsValue::IstTemp, EmsValue::WW);
    parseInteger(10, 3, EmsValue::WarmwasserbereitungsZeit, EmsValue::None);
    parseInteger(13, 3, EmsValue::WarmwasserBereitungen, EmsValue::None);

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
    parseBool(1, 0, EmsValue::KesselSchalter, EmsValue::WW);
    parseNumeric(2, 1, 1, EmsValue::SetTemp, EmsValue::WW);
    parseNumeric(8, 1, 1, EmsValue::DesinfektionsTemp, EmsValue::WW);
    parseEnum(7,EmsValue::Schaltpunkte, EmsValue::Zirkulation);
}

void
EmsMessage::parseUBAErrorMessage()
{
    size_t start;

    if (m_offset % sizeof(EmsProto::ErrorRecord)) {
	start = ((m_offset / sizeof(EmsProto::ErrorRecord)) + 1) * sizeof(EmsProto::ErrorRecord);
    } else {
	start = m_offset;
    }

    while (canAccess(start, sizeof(EmsProto::ErrorRecord))) {
	EmsProto::ErrorRecord *record = (EmsProto::ErrorRecord *) &m_data.at(start - m_offset);
	unsigned int index = start / sizeof(EmsProto::ErrorRecord);
	EmsValue::ErrorEntry entry = { m_type, index, *record };

	m_valueHandler(EmsValue(EmsValue::Fehler, EmsValue::None, entry));
	start += sizeof(EmsProto::ErrorRecord);
    }
}

void
EmsMessage::parseUBAParametersMessage()
{
    parseBool(0, 1, EmsValue::KesselSchalter, EmsValue::Kessel);
    parseNumeric(1, 1, 1, EmsValue::SetTemp, EmsValue::Kessel);
    parseNumeric(2, 1, 1, EmsValue::MaxModulation, EmsValue::Brenner);
    parseNumeric(3, 1, 1, EmsValue::MinModulation, EmsValue::Brenner);
    parseNumeric(4, 1, 1, EmsValue::EinschaltHysterese, EmsValue::Kessel);
    parseNumeric(5, 1, 1, EmsValue::AusschaltHysterese, EmsValue::Kessel);
    parseInteger(6, 1, EmsValue::AntipendelZeit, EmsValue::None);
    parseInteger(8, 1, EmsValue::NachlaufZeit, EmsValue::KesselPumpe);
    parseNumeric(9, 1, 1, EmsValue::MaxModulation, EmsValue::KesselPumpe);
    parseNumeric(10, 1, 1, EmsValue::MinModulation, EmsValue::KesselPumpe);
}

void
EmsMessage::parseRCTimeMessage()
{
    if (canAccess(0, sizeof(EmsProto::SystemTimeRecord))) {
	EmsProto::SystemTimeRecord *record = (EmsProto::SystemTimeRecord *) &m_data.at(0);
	EmsValue value(EmsValue::SystemZeit, EmsValue::None, *record);
	m_valueHandler(value);
    }
}

void
EmsMessage::parseRCWWOpmodeMessage()
{
    parseBool(0, 1, EmsValue::EigenesProgrammAktiv, EmsValue::WW);
    parseBool(1, 1, EmsValue::EigenesProgrammAktiv, EmsValue::Zirkulation);

    parseEnum(2, EmsValue::Betriebsart, EmsValue::Zirkulation);
    parseEnum(3, EmsValue::Betriebsart, EmsValue::WW);

    parseBool(4, 1, EmsValue::Desinfektion, EmsValue::WW);
    parseEnum(5, EmsValue::DesinfektionTag, EmsValue::WW);
    parseInteger(6, 1, EmsValue::DesinfektionStunde, EmsValue::WW);
    parseNumeric(8, 1, 1, EmsValue::MaxTemp, EmsValue::WW);
    parseBool(9 ,1, EmsValue::EinmalLadungsLED, EmsValue::WW);
}

void
EmsMessage::parseRCSystemParameterMessage()
{
    parseNumeric(5, 1, 1, EmsValue::MinTemp, EmsValue::Aussen);
    parseEnum(6, EmsValue::GebaeudeArt, EmsValue::None);
    parseBool(21 ,1, EmsValue::ATDaempfung, EmsValue::None);
    
}

void
EmsMessage::parseRCHKOpmodeMessage(EmsValue::SubType subtype)
{
    parseEnum(0, EmsValue::HeizArt, subtype);
    parseNumeric(1, 1, 2, EmsValue::TagTemp, subtype);
    parseNumeric(2, 1, 2, EmsValue::NachtTemp, subtype);
    parseNumeric(3, 1, 2, EmsValue::UrlaubTemp, subtype);
    parseNumeric(4, 1, 2, EmsValue::RaumEinfluss, subtype);
    parseNumeric(6, 1, 2, EmsValue::RaumOffset, subtype);
    parseEnum(7, EmsValue::Betriebsart, subtype);
    parseNumeric(16, 1, 1, EmsValue::MinTemp, subtype);
    parseNumeric(35, 1, 1, EmsValue::MaxTemp, subtype);
    parseBool(19, 1, EmsValue::SchaltzeitOptimierung, subtype);
    parseNumeric(22, 1, 1, EmsValue::SchwelleSommerWinter, subtype);
    parseNumeric(23, 1, 1, EmsValue::FrostSchutzTemp, subtype);
    parseEnum(25, EmsValue::RegelungsArt, subtype);
    parseEnum(28, EmsValue::Frostschutz, subtype);
    parseEnum(32, EmsValue::HeizSystem, subtype);
    parseEnum(33, EmsValue::FuehrungsGroesse, subtype);
    parseNumeric(36, 1, 1, EmsValue::AuslegungsTemp, subtype);
    parseNumeric(37, 1, 2, EmsValue::RaumUebersteuerTemp, subtype);
    parseNumeric(39, 1, 1, EmsValue::AbsenkungsSchwellenTemp, subtype);
    parseEnum(41, EmsValue::UrlaubAbsenkungsArt, subtype);
    
}

void
EmsMessage::parseRCHKScheduleMessage(EmsValue::SubType subtype)
{
    // ToDo: Implement
}

void
EmsMessage::parseRCOutdoorTempMessage()
{
    parseNumeric(0, 1, 1, EmsValue::GedaempfteTemp, EmsValue::Aussen);
}

void
EmsMessage::parseRCHKMonitorMessage(EmsValue::SubType subtype)
{
    parseNumeric(2, 1, 2, EmsValue::SollTemp, EmsValue::Raum);
    parseNumeric(3, 2, 10, EmsValue::IstTemp, EmsValue::Raum);

    if (canAccess(7, 3)) {
	EmsValue value(EmsValue::HKKennlinie, subtype, m_data[7 - m_offset],
		m_data[8 - m_offset], m_data[9 - m_offset]);
	m_valueHandler(value);
    }

    parseNumeric(14, 1, 1, EmsValue::SollTemp, subtype);
    parseInteger(5, 1, EmsValue::EinschaltoptimierungsZeit, subtype);
    parseInteger(6, 1, EmsValue::AusschaltoptimierungsZeit, subtype);

    if (canAccess(15, 1) && (m_data[15 - m_offset] & 1) == 0) {
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
    parseNumeric(0, 2, 10, EmsValue::IstTemp, EmsValue::HK1);

    /* Byte 2 = 0 -> Pumpe aus, 100 = 0x64 -> Pumpe an */
    parseBool(2, 2, EmsValue::PumpeAktiv, EmsValue::HK1);
}

void
EmsMessage::parseWMTemp2Message()
{
    parseNumeric(0, 2, 10, EmsValue::IstTemp, EmsValue::HK1);
}

void
EmsMessage::parseMMTempMessage()
{
    parseNumeric(0, 1, 1, EmsValue::SollTemp, EmsValue::HK2);
    parseNumeric(1, 2, 10, EmsValue::IstTemp, EmsValue::HK2);
    parseNumeric(3, 1, 1, EmsValue::Mischersteuerung, EmsValue::None);

    /* Byte 3 = 0 -> Pumpe aus, 100 = 0x64 -> Pumpe an */
    parseBool(3, 2, EmsValue::PumpeAktiv, EmsValue::HK2);
}

