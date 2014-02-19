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
	{ EmsValue::MaxTemp, "Maximale Temperatur" },
	{ EmsValue::GedaempfteTemp, "Temperatur (gedämpft)" },
	{ EmsValue::DesinfektionsTemp, "Desinfektionstemperatur" },
	{ EmsValue::TemperaturAenderung, "Temperaturänderung" },
	{ EmsValue::Mischersteuerung, "Mischersteuerung" },
	{ EmsValue::Flammenstrom, "Flammenstrom" },
	{ EmsValue::Systemdruck, "Systemdruck" },
	{ EmsValue::BetriebsZeit, "Betriebszeit" },
	{ EmsValue::SollModulation, "Sollwert Modulation" },
	{ EmsValue::IstModulation, "Istwert Modulation" },
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
	{ EmsValue::NachlaufZeit, "Nachlaufzeit" },
	{ EmsValue::DesinfektionStunde, "Thermische Desinfektion Stunde" },

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
	{ EmsValue::KesselSchalter, "per Kesselschalter freigegeben" },
	{ EmsValue::EigenesProgrammAktiv, "Eigenes Programm aktiv" },
	{ EmsValue::EinmalLadungsLED, "Einmalladungs-LED" },
	{ EmsValue::Desinfektion, "Thermische Desinfektion" },

	{ EmsValue::WWSystemType, "WW-System-Typ" },
	{ EmsValue::Betriebsart, "Betriebsart" },
	{ EmsValue::Wartungsmeldungen, "Wartungsmeldungen" },
	{ EmsValue::WartungFaellig, "Wartung f�llig?" },
	{ EmsValue::DesinfektionTag, "Thermische Desinfektion Tag" },
	{ EmsValue::Schaltpunkte, "Schaltpunkte" },

	{ EmsValue::HKKennlinie, "Kennlinie" },
	{ EmsValue::Fehler, "Fehler" },

	{ EmsValue::ServiceCode, "Servicecode" },
	{ EmsValue::FehlerCode, "Fehlercode" }
    };
    static const std::map<EmsValue::SubType, const char *> SUBTYPEMAPPING = {
	{ EmsValue::HK1, "HK1" },
	{ EmsValue::HK2, "HK2" },
	{ EmsValue::HK3, "HK3" },
	{ EmsValue::HK4, "HK4" },
	{ EmsValue::Kessel, "Kessel" },
	{ EmsValue::KesselPumpe, "Kesselpumpe" },
	{ EmsValue::Ruecklauf, "Rücklauf" },
	{ EmsValue::Waermetauscher, "Waermetauscher" },
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
	{ EmsValue::MaxTemp, "°C" },
	{ EmsValue::GedaempfteTemp, "°C" },
	{ EmsValue::DesinfektionsTemp, "°C" },
	{ EmsValue::TemperaturAenderung, "K/min" },
	{ EmsValue::EinschaltHysterese, "K" },
	{ EmsValue::AusschaltHysterese, "K" },
	{ EmsValue::IstModulation, "%" },
	{ EmsValue::SollModulation, "%" },
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
	{ EmsValue::NachlaufZeit, "min" },
	{ EmsValue::DesinfektionStunde, "h" }
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

    static const std::map<uint8_t, const char *> MAINTENANCEMESSAGESMAPPING = {
	{ 0, "keine" },
	{ 1, "nach Betriebsstunden" }, 
	{ 2, "nach Datum" }
    };

    static const std::map<uint8_t, const char *> MAINTENANCENEEDEDMAPPING = {
	{ 0, "nein" },
	{ 3, "ja, wegen Betriebsstunden" }, 
	{ 8, "ja, wegen Datum" }
    };

    static const std::map<uint8_t, const char *> ERRORTYPEMAPPING = {
	{ 0x10, "Blockierender Fehler" },
	{ 0x11, "Verriegelnder Fehler" },
	{ 0x12, "Anlagenfehler" },
	{ 0x13, "Anlagenfehler" }
    };

    static const std::map<uint8_t, const char *> OPMODEMAPPING = {
        { 0, "staendig aus" }, { 1, "staendig an" }, { 2, "Automatik" }
    };

    static const std::map<uint8_t, const char *> DAYMAPPING = {
        { 0, "Montag" }, { 1, "Dienstag" }, { 2, "Mittwoch"}, { 3, "Donnerstag" },
        { 4, "Freitag" }, { 5, "Samstag" }, { 6, "Sonntag" }, { 7, "Taeglich" }
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
	    stream << std::setprecision(10) << boost::get<double>(value.getValue());
	    if (unitIter != UNITMAPPING.end()) {
		stream << " " << unitIter->second;
	    }
	    break;
	}
	case EmsValue::Boolean:
	    stream << (boost::get<bool>(value.getValue()) ? "AN" : "AUS");
	    break;
	case EmsValue::Enumeration: {
	    const std::map<uint8_t, const char *> *map = NULL;
	    uint8_t enumValue = boost::get<uint8_t>(value.getValue());
	    switch (value.getType()) {
		case EmsValue::WWSystemType: map = &WWSYSTEMMAPPING; break;
		case EmsValue::Schaltpunkte: map = &ZIRKSPMAPPING; break;
		case EmsValue::Wartungsmeldungen: map = &MAINTENANCEMESSAGESMAPPING; break;
                case EmsValue::WartungFaellig: map = &MAINTENANCENEEDEDMAPPING; break;
                case EmsValue::Betriebsart: map = &OPMODEMAPPING; break;
                case EmsValue::DesinfektionTag: map = &DAYMAPPING; break;
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
	    std::vector<uint8_t> kennlinie = boost::get<std::vector<uint8_t> >(value.getValue());
	    stream << "-10 °C: " << (unsigned int) kennlinie[0] << " °C / ";
	    stream << "0 °C: " << (unsigned int) kennlinie[1] << " °C / ";
	    stream << "10 °C: " << (unsigned int) kennlinie[2] << " °C";
	    break;
	}
	case EmsValue::Error: {
	    EmsValue::ErrorEntry entry = boost::get<EmsValue::ErrorEntry>(value.getValue());
	    EmsProto::ErrorRecord& record = entry.record;
	    stream << ERRORTYPEMAPPING.at(entry.type) << " " << entry.index << ": ";
	    if (record.errorAscii[0] == 0) {
		stream << "Empty" << std::endl;
	    } else {
		stream << "Source " << std::hex << (unsigned int) record.source << ", error ";
		stream << std::dec << record.errorAscii[0] << record.errorAscii[1] << ", code ";
		stream << __be16_to_cpu(record.code_be16) << ", duration ";
		stream << __be16_to_cpu(record.durationMinutes_be16) << " minutes" << std::endl;
		if (record.hasDate) {
		    stream << "; error date " << (2000 + record.year) << "-";
		    stream << record.month << "-" << record.day << "; time ";
		    stream << record.hour << ":" << record.minute;
		}
	    }
	    break;
	}
	case EmsValue::Formatted:
	    stream << boost::get<std::string>(value.getValue());
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
