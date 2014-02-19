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
#include <iomanip>
#include <asm/byteorder.h>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include "IoHandler.h"
#include "Options.h"

IoHandler::IoHandler(Database& db) :
    boost::asio::io_service(),
    m_active(true),
    m_db(db),
    m_state(Syncing),
    m_pos(0)
{
    /* pre-alloc buffer to avoid reallocations */
    m_data.reserve(256);

    m_valueCb = boost::bind(&IoHandler::handleValue, this, _1);
}

IoHandler::~IoHandler()
{
}

void
IoHandler::readComplete(const boost::system::error_code& error,
			size_t bytesTransferred)
{
    size_t pos = 0;
    DebugStream& debug = Options::ioDebug();

    if (error) {
	doClose(error);
	return;
    }

    if (debug) {
	debug << "IO: Got bytes ";
	for (size_t i = 0; i < bytesTransferred; i++) {
	    debug << std::setfill('0') << std::setw(2)
		  << std::showbase << std::hex
		  << (unsigned int) m_recvBuffer[i] << " ";
	}
	debug << std::endl;
    }

    while (pos < bytesTransferred) {
	unsigned char dataByte = m_recvBuffer[pos++];

	switch (m_state) {
	    case Syncing:
		if (m_pos == 0 && dataByte == 0xaa) {
		    m_pos = 1;
		} else if (m_pos == 1 && dataByte == 0x55) {
		    m_state = Length;
		    m_pos = 0;
		} else {
		    m_pos = 0;
		}
		break;
	    case Length:
		m_state = Data;
		m_pos = 0;
		m_length = dataByte;
		m_checkSum = 0;
		break;
	    case Data:
		m_data.push_back(dataByte);
		m_checkSum ^= dataByte;
		m_pos++;
		if (m_pos == m_length) {
		    m_state = Checksum;
		}
		break;
	    case Checksum:
		if (m_checkSum == dataByte) {
		    EmsMessage message(m_valueCb, m_data);
		    message.handle();
		    if (message.getDestination() == EmsProto::addressPC && m_pcMessageCallback) {
			m_pcMessageCallback(message);
		    }
		}
		m_data.clear();
		m_state = Syncing;
		m_pos = 0;
		break;
	}
    }

    readStart();
}

void
IoHandler::doClose(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	std::cerr << "Error: " << error.message() << std::endl;
    }

    doCloseImpl();
    m_active = false;
    stop();
}

static void
printDescriptive(std::ostream& stream, const EmsValue& value)
{
    static const std::map<EmsValue::Type, const char *> TYPEMAPPING = {
	{ EmsValue::SollTemp, "Solltemperatur" },
	{ EmsValue::IstTemp, "Isttemperatur" },
	{ EmsValue::SetTemp, "Temperatureinstellung" },
	{ EmsValue::GedaempfteTemp, "Temperatur (gedämpft)" },
	{ EmsValue::TemperaturAenderung, "Temperaturänderung" },
	{ EmsValue::Mischersteuerung, "Mischersteuerung" },
	{ EmsValue::MomLeistung, "Momentane Leistung" },
	{ EmsValue::MaxLeistung, "Maximale Leistung" },
	{ EmsValue::Flammenstrom, "Flammenstrom" },
	{ EmsValue::Systemdruck, "Systemdruck" },
	{ EmsValue::BetriebsZeit, "Betriebszeit" },
	{ EmsValue::PumpenModulation, "Pumpenmodulation" },
	{ EmsValue::MinModulation, "Min. Modulation" },
	{ EmsValue::MaxModulation, "Max. Modulation" },
	{ EmsValue::HeizZeit, "Heizzeit" },
	{ EmsValue::WarmwasserbereitungsZeit, "WW-Bereitungszeit" },
	{ EmsValue::Brennerstarts, "Brennerstarts" },
	{ EmsValue::WarmwasserBereitungen, "WW-Bereitungen " },
	{ EmsValue::EinschaltoptimierungsZeit, "Einschaltoptimierungszeit" },
	{ EmsValue::AusschaltoptimierungsZeit, "Abschaltoptimierungszeit" },
	{ EmsValue::EinschaltHysterese, "Einschalthysterese" },
	{ EmsValue::AusschaltHysterese, "Abschalthysterese" },
	{ EmsValue::AntipendelZeit, "Antipendelzeit" },
	{ EmsValue::PumpenNachlaufZeit, "Pumpennachlaufzeit" },

	{ EmsValue::FlammeAktiv, "Flamme" },
	{ EmsValue::BrennerAktiv, "Brenner" },
	{ EmsValue::ZuendungAktiv, "Zündung" },
	{ EmsValue::PumpeAktiv, "Pumpe" },
	{ EmsValue::ZirkulationAktiv, "Zirkulation" },
	{ EmsValue::DreiWegeVentilAufWW, "3-Wege-Ventil auf WW" },
	{ EmsValue::EinmalLadungAktiv, "Einmalladung" },
	{ EmsValue::DesinfektionAktiv, "Therm. Desinfektion" },
	{ EmsValue::NachladungAktiv, "Nachladung" },
	{ EmsValue::WarmwasserBereitung, "WW-Bereitung" },
	{ EmsValue::WarmwasserTempOK, "WW-Temperatur OK" },
	{ EmsValue::Automatikbetrieb, "Automatikbetrieb" },
	{ EmsValue::Tagbetrieb, "Tagbetrieb" },
	{ EmsValue::Sommerbetrieb, "Sommerbetrieb" },
	{ EmsValue::Ausschaltoptimierung, "Ausschaltoptimierung" },
	{ EmsValue::Einschaltoptimierung, "Einschaltoptimierung" },
	{ EmsValue::Estrichtrocknung, "Estrichtrocknung" },
	{ EmsValue::WWVorrang, "WW-Vorrang" },
	{ EmsValue::Ferien, "Ferienbetrieb" },
	{ EmsValue::Party, "Partybetrieb" },
	{ EmsValue::Frostschutz, "Frostschutz" },
	{ EmsValue::SchaltuhrEin, "Schaltuhr aktiv" },

	{ EmsValue::WWSystemType, "WW-System-Typ" },
	{ EmsValue::Schaltpunkte, "Schaltpunkte" },

	{ EmsValue::HKKennlinie, "Kennlinie" },
	{ EmsValue::Fehler, "Fehler" },
	{ EmsValue::SystemZeit, "Systemzeit" },

	{ EmsValue::ServiceCode, "Servicecode" },
	{ EmsValue::FehlerCode, "Fehlercode" }
    };
    static const std::map<EmsValue::SubType, const char *> SUBTYPEMAPPING = {
	{ EmsValue::HK1, "HK1" },
	{ EmsValue::HK2, "HK2" },
	{ EmsValue::HK3, "HK3" },
	{ EmsValue::HK4, "HK4" },
	{ EmsValue::Kessel, "Kessel" },
	{ EmsValue::Ruecklauf, "Rücklauf" },
	{ EmsValue::WW, "Warmwasser" },
	{ EmsValue::Zirkulation, "Zirkulation" },
	{ EmsValue::Raum, "Raum" },
	{ EmsValue::Aussen, "Außen" },
	{ EmsValue::Abgas, "Abgas" },
    };
    static const std::map<EmsValue::Type, const char *> UNITMAPPING = {
	{ EmsValue::SollTemp, "°C" },
	{ EmsValue::IstTemp, "°C" },
	{ EmsValue::SetTemp, "°C" },
	{ EmsValue::GedaempfteTemp, "°C" },
	{ EmsValue::TemperaturAenderung, "K/min" },
	{ EmsValue::EinschaltHysterese, "K" },
	{ EmsValue::AusschaltHysterese, "K" },
	{ EmsValue::MomLeistung, "%" },
	{ EmsValue::MaxLeistung, "%" },
	{ EmsValue::PumpenModulation, "%" },
	{ EmsValue::MinModulation, "%" },
	{ EmsValue::MaxModulation, "%" },
	{ EmsValue::Flammenstrom, "µA" },
	{ EmsValue::Systemdruck, "bar" },
	{ EmsValue::BetriebsZeit, "min" },
	{ EmsValue::HeizZeit, "min" },
	{ EmsValue::WarmwasserbereitungsZeit, "min" },
	{ EmsValue::EinschaltoptimierungsZeit, "min" },
	{ EmsValue::AusschaltoptimierungsZeit, "min" },
	{ EmsValue::AntipendelZeit, "min" },
	{ EmsValue::PumpenNachlaufZeit, "min" }
    };

    static const std::map<uint8_t, const char *> WWSYSTEMMAPPING = {
	{ EmsProto::WWSystemNone, "keins" },
	{ EmsProto::WWSystemDurchlauf, "Durchlauferhitzer" },
	{ EmsProto::WWSystemKlein, "klein" },
	{ EmsProto::WWSystemGross, "groß" },
	{ EmsProto::WWSystemSpeicherlade, "Speicherladesystem" }
    };

    static const std::map<uint8_t, const char *> ZIRKSPMAPPING = {
	{ 0, "aus" },
	{ 1, "1x 3min" }, { 2, "2x 3min" }, { 3, "3x 3min" },
	{ 4, "4x 3min" }, { 5, "5x 3min" }, { 6, "6x 3min" },
	{ 7, "dauerhaft an" }
    };

    static const std::map<uint8_t, const char *> ERRORTYPEMAPPING = {
	{ 0x10, "Blockierender Fehler" },
	{ 0x11, "Verriegelnder Fehler" },
	{ 0x12, "Anlagenfehler" },
	{ 0x13, "Anlagenfehler" }
    };

    static const std::map<uint8_t, const char *> WEEKDAYMAPPING = {
	{ 0, "Montag" }, { 1, "Dienstag" }, { 2, "Mittwoch" }, { 3, "Donnerstag" },
	{ 4, "Freitag" }, { 5, "Samstag" }, { 6, "Sonntag" }
    };

    auto typeIter = TYPEMAPPING.find(value.getType());
    const char *type = typeIter != TYPEMAPPING.end() ? typeIter->second : NULL;
    auto subtypeIter = SUBTYPEMAPPING.find(value.getSubType());
    const char *subtype = subtypeIter != SUBTYPEMAPPING.end() ? subtypeIter->second : NULL;

    if (subtype) {
	stream << subtype;
	if (type) {
	    stream << "-";
	}
    }
    if (type) {
	stream << type;
    } else {
	stream << "???";
    }
    stream << " = ";

    switch (value.getReadingType()) {
	case EmsValue::Numeric: {
	    auto unitIter = UNITMAPPING.find(value.getType());
	    stream << value.getValue<float>();
	    if (unitIter != UNITMAPPING.end()) {
		stream << " " << unitIter->second;
	    }
	    break;
	}
	case EmsValue::Boolean:
	    stream << (value.getValue<bool>() ? "AN" : "AUS");
	    break;
	case EmsValue::Enumeration: {
	    const std::map<uint8_t, const char *> *map = NULL;
	    uint8_t enumValue = value.getValue<uint8_t>();
	    switch (value.getType()) {
		case EmsValue::WWSystemType: map = &WWSYSTEMMAPPING; break;
		case EmsValue::Schaltpunkte: map = &ZIRKSPMAPPING; break;
		default: break;
	    }
	    if (map && map->find(enumValue) != map->end()) {
		stream << map->at(enumValue);
	    } else {
		stream << "??? (" << (unsigned int) enumValue << ")";
	    }
	    break;
	}
	case EmsValue::Kennlinie: {
	    std::vector<uint8_t> kennlinie = value.getValue<std::vector<uint8_t> >();
	    stream << boost::format("-10 °C: %d °C / 0 °C: %d °C / 10 °C: %d °C")
		    % (unsigned int) kennlinie[0] % (unsigned int) kennlinie[1]
		    % (unsigned int) kennlinie[2];
	    break;
	}
	case EmsValue::Error: {
	    EmsValue::ErrorEntry entry = value.getValue<EmsValue::ErrorEntry>();
	    EmsProto::ErrorRecord& record = entry.record;
	    stream << ERRORTYPEMAPPING.at(entry.type) << " " << entry.index << ": ";
	    if (record.errorAscii[0] == 0) {
		stream << "Leer" << std::endl;
	    } else {
		boost::format f("Quelle 0x%02x, Fehler %c%c, Code %d, Dauer %d Minuten");
		f % (unsigned int) record.source % record.errorAscii[0] % record.errorAscii[1];
		f % __be16_to_cpu(record.code_be16) % __be16_to_cpu(record.durationMinutes_be16);
		stream << f << std::endl;
		if (record.time.valid) {
		    boost::format ft("%d.%d.%d %d:%02d");
		    ft % (unsigned int) record.time.day % (unsigned int) record.time.month;
		    ft % (2000 + record.time.year);
		    ft % (unsigned int) record.time.hour % (unsigned int) record.time.minute;
		    stream << "; Zeitpunkt " << ft << std::endl;
		}
	    }
	    break;
	}
	case EmsValue::SystemTime: {
	    EmsProto::SystemTimeRecord record = value.getValue<EmsProto::SystemTimeRecord>();
	    auto dayIter = WEEKDAYMAPPING.find(record.dayOfWeek);

	    stream << boost::format("%d.%d.%d")
		    % (2000 + record.common.year)
		    % (unsigned int) record.common.month % (unsigned int) record.common.day;

	    if (dayIter != WEEKDAYMAPPING.end()) {
		stream << " (" << dayIter->second << ")";
	    }
	    stream << ", " << boost::format("%d:%02d:%02d")
		    % (unsigned int) record.common.hour % (unsigned int) record.common.minute
		    % (unsigned int) record.second;
	    if (record.running || record.dcf || record.dst) {
		bool hasOutput = false;
		stream << " (";
		if (record.running) {
		    stream << "läuft";
		    hasOutput = true;
		}
		if (record.dcf) {
		    if (hasOutput) stream << ", ";
		    stream << "DCF";
		    hasOutput = true;
		}
		if (record.dst) {
		    if (hasOutput) stream << ", ";
		    stream << "Sommerzeit";
		    hasOutput = true;
		}
		stream << ")";
	    }
	    break;
	}
	case EmsValue::Formatted:
	    stream << value.getValue<std::string>();
	    break;
    }
}

void
IoHandler::handleValue(const EmsValue& value)
{
    if (Options::dataDebug()) {
	Options::dataDebug() << "DATA: ";
	printDescriptive(Options::dataDebug(), value);
	Options::dataDebug() << std::endl;
    }
    if (m_valueCallback) {
	m_valueCallback(value);
    }
    m_db.handleValue(value);
}
