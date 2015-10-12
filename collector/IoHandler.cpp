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
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include "ByteOrder.h"
#include "IoHandler.h"
#include "Options.h"

IoHandler::IoHandler(Database& db, ValueCache& cache) :
    boost::asio::io_service(),
    m_active(true),
    m_db(db),
    m_cache(cache),
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
	{ EmsValue::MinTemp, "Minimale Temperatur" },
	{ EmsValue::MaxTemp, "Maximale Temperatur" },
	{ EmsValue::TagTemp, "Tagtemperatur" },
	{ EmsValue::NachtTemp, "Nachttemperatur" },
	{ EmsValue::UrlaubTemp, "Urlaubstemperatur" },
	{ EmsValue::RaumEinfluss, "Max. Raumeinfluss" },
	{ EmsValue::RaumOffset, "Raumoffset" },
	{ EmsValue::SchwelleSommerWinter, "Schwelle Sommer/Winter" },
	{ EmsValue::FrostSchutzTemp, "Frostschutztemperatur" },
	{ EmsValue::AuslegungsTemp, "Auslegungstemperatur" },
	{ EmsValue::RaumUebersteuerTemp, "Temporäre Raumtemperaturübersteuerung" },
	{ EmsValue::AbsenkungsSchwellenTemp, "Schwellentemperatur Außenhaltbetrieb" },
	{ EmsValue::UrlaubAbsenkungsSchwellenTemp, "Schwellentemperatur Außenhaltbetrieb Urlaub" },
	{ EmsValue::AbsenkungsAbbruchTemp, "Nachtabsenkung abbrechen unterhalb" },
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
	{ EmsValue::EinschaltHysterese, "Einschalthysterese" },
	{ EmsValue::AusschaltHysterese, "Abschalthysterese" },
	{ EmsValue::DurchflussMenge, "Durchflussmenge" },
	{ EmsValue::SollLeistung, "Angeforderte Leistung" },

	{ EmsValue::HeizZeit, "Heizzeit" },
	{ EmsValue::WarmwasserbereitungsZeit, "WW-Bereitungszeit" },
	{ EmsValue::Brennerstarts, "Brennerstarts" },
	{ EmsValue::WarmwasserBereitungen, "WW-Bereitungen " },
	{ EmsValue::EinschaltoptimierungsZeit, "Einschaltoptimierungszeit" },
	{ EmsValue::AusschaltoptimierungsZeit, "Abschaltoptimierungszeit" },
	{ EmsValue::AntipendelZeit, "Antipendelzeit" },
	{ EmsValue::NachlaufZeit, "Nachlaufzeit" },
	{ EmsValue::DesinfektionStunde, "Thermische Desinfektion Stunde" },
	{ EmsValue::HektoStundenVorWartung, "Wartungsintervall in 100h" },
	{ EmsValue::PausenZeit, "restl. Pausenzeit" },
	{ EmsValue::PartyZeit, "restl. Partyzeit" },

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
	{ EmsValue::Urlaub, "Urlaubsbetrieb" },
	{ EmsValue::Party, "Partybetrieb" },
	{ EmsValue::Pause, "Pausebetrieb" },
	{ EmsValue::Frostschutzbetrieb, "Frostschutzbetrieb" },
	{ EmsValue::SchaltuhrEin, "Schaltuhr aktiv" },
	{ EmsValue::KesselSchalter, "per Kesselschalter freigegeben" },
	{ EmsValue::EigenesProgrammAktiv, "Eigenes Programm aktiv" },
	{ EmsValue::EinmalLadungsLED, "Einmalladungs-LED" },
	{ EmsValue::Desinfektion, "Thermische Desinfektion" },
	{ EmsValue::ATDaempfung, "Dämpfung Außentemperatur" },
	{ EmsValue::SchaltzeitOptimierung, "Schaltzeitoptimierung" },
	{ EmsValue::Fuehler1Defekt, "Fühler 1 defekt" },
	{ EmsValue::Fuehler2Defekt, "Fühler 2 defekt" },
	{ EmsValue::Manuellbetrieb, "Manueller Betrieb" },
	{ EmsValue::Stoerung, "Störung" },
	{ EmsValue::StoerungDesinfektion, "Störung Desinfektion" },
	{ EmsValue::Ladevorgang, "Ladevorgang" },

	{ EmsValue::WWSystemType, "WW-System-Typ" },
	{ EmsValue::Betriebsart, "Betriebsart" },
	{ EmsValue::Wartungsmeldungen, "Wartungsmeldungen" },
	{ EmsValue::WartungFaellig, "Wartung fällig?" },
	{ EmsValue::DesinfektionTag, "Thermische Desinfektion Tag" },
	{ EmsValue::Schaltpunkte, "Schaltpunkte" },
	{ EmsValue::GebaeudeArt, "Gebäudeart" },
	{ EmsValue::RegelungsArt, "Regelungsart" },
	{ EmsValue::HeizSystem, "Heizsystem" },
	{ EmsValue::FuehrungsGroesse, "Führungsgröße" },
	{ EmsValue::Frostschutz, "Frostschutz" },
	{ EmsValue::UrlaubAbsenkungsArt, "Urlaubsabsenkungsart" },
	{ EmsValue::FBTyp, "Fernbedienungstyp" },

	{ EmsValue::HKKennlinie, "Kennlinie" },
	{ EmsValue::Fehler, "Fehler" },
	{ EmsValue::SystemZeit, "Systemzeit" },
	{ EmsValue::Wartungstermin, "Wartungstermin" },

	{ EmsValue::ServiceCode, "Servicecode" },
	{ EmsValue::FehlerCode, "Fehlercode" }
    };
    static const std::map<EmsValue::SubType, const char *> SUBTYPEMAPPING = {
	{ EmsValue::HK1, "HK1" },
	{ EmsValue::HK2, "HK2" },
	{ EmsValue::HK3, "HK3" },
	{ EmsValue::HK4, "HK4" },
	{ EmsValue::Kessel, "Kessel" },
	{ EmsValue::Brenner, "Brenner" },
	{ EmsValue::KesselPumpe, "Kesselpumpe" },
	{ EmsValue::Ruecklauf, "Rücklauf" },
	{ EmsValue::Waermetauscher, "Wärmetauscher" },
	{ EmsValue::WW, "Warmwasser" },
	{ EmsValue::Zirkulation, "Zirkulation" },
	{ EmsValue::Raum, "Raum" },
	{ EmsValue::Aussen, "Außen" },
	{ EmsValue::Abgas, "Abgas" },
	{ EmsValue::Ansaugluft, "Ansaugluft" },
	{ EmsValue::Solar, "Solar" },
	{ EmsValue::SolarSpeicher, "Solarspeicher" },
	{ EmsValue::SolarKollektor, "Solarkollektor" }
    };
    static const std::map<EmsValue::Type, const char *> UNITMAPPING = {
	{ EmsValue::SollTemp, "°C" },
	{ EmsValue::IstTemp, "°C" },
	{ EmsValue::SetTemp, "°C" },
	{ EmsValue::MinTemp, "°C" },
	{ EmsValue::MaxTemp, "°C" },
	{ EmsValue::TagTemp, "°C" },
	{ EmsValue::NachtTemp, "°C" },
	{ EmsValue::UrlaubTemp, "°C" },
	{ EmsValue::RaumEinfluss, "K" },
	{ EmsValue::RaumOffset, "K" },
	{ EmsValue::SchwelleSommerWinter, "°C" },
	{ EmsValue::FrostSchutzTemp, "°C" },
	{ EmsValue::AuslegungsTemp, "°C" },
	{ EmsValue::RaumUebersteuerTemp, "°C" },
	{ EmsValue::AbsenkungsSchwellenTemp, "°C" },
	{ EmsValue::UrlaubAbsenkungsSchwellenTemp, "°C" },
	{ EmsValue::AbsenkungsAbbruchTemp, "°C" },
	{ EmsValue::GedaempfteTemp, "°C" },
	{ EmsValue::DesinfektionsTemp, "°C" },
	{ EmsValue::TemperaturAenderung, "K/min" },
	{ EmsValue::EinschaltHysterese, "K" },
	{ EmsValue::AusschaltHysterese, "K" },
	{ EmsValue::DurchflussMenge, "l/min" },
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
	{ EmsValue::DesinfektionStunde, "h" },
	{ EmsValue::PausenZeit, "h" },
	{ EmsValue::PartyZeit, "h" }
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
	{ 0x10, "Verriegelnder Fehler" },
	{ 0x11, "Blockierender Fehler" },
	{ 0x12, "Anlagenfehler" },
	{ 0x13, "Zurückgesetzter Anlagenfehler" }
    };

    static const std::map<uint8_t, const char *> WEEKDAYMAPPING = {
	{ 0, "Montag" }, { 1, "Dienstag" }, { 2, "Mittwoch" }, { 3, "Donnerstag" },
	{ 4, "Freitag" }, { 5, "Samstag" }, { 6, "Sonntag" }
    };

    static const std::map<uint8_t, const char *> OPMODEMAPPING = {
	{ 0, "ständig aus" }, { 1, "ständig an" }, { 2, "Automatik" }
    };


    static const std::map<uint8_t, const char *> BUILDINGTYPEMAPPING = {
	{ 0, "leicht" }, { 1, "mittel" }, { 2, "schwer" }
    };

    static const std::map<uint8_t, const char *> HEATINGTYPEMAPPING = {
	{ 1, "Heizkörper" }, { 2, "Konvektor" }, { 3, "Fußboden" },
    };

    static const std::map<uint8_t, const char *> CONTROLTYPEMAPPING = {
	{ 0, "Abschalt" }, { 1, "Reduziert" }, { 2, "Raumhalt" }, { 3, "Außenhalt" }
    };

    static const std::map<uint8_t, const char *> FROSTPROTECTMAPPING = {
	{ 0, "kein" }, { 1, "Außentemperatur" }, { 2, "Raumtemperatur 5 Grad" }
    };

    static const std::map<uint8_t, const char *> RELEVANTVALUEMAPPING = {
	{ 0, "außentemperaturgeführt" }, { 1, "raumtemperaturgeführt" }
    };

    static const std::map<uint8_t, const char *> VACATIONREDUCTIONMAPPING = {
	{ 3, "Außenhalt" }, { 2, "Raumhalt" }
    };

    static const std::map<uint8_t, const char *> REMOTETYPEMAPPING = {
	{ 0, "Keine" }, { 1, "RC20" }, { 2, "RC3x" }
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
	case EmsValue::Numeric:
	case EmsValue::Integer: {
	    if (value.isValid()) {
		if (value.getReadingType() == EmsValue::Numeric) {
		    stream << value.getValue<float>();
		} else {
		    stream << value.getValue<unsigned int>();
		}
		auto unitIter = UNITMAPPING.find(value.getType());
		if (unitIter != UNITMAPPING.end()) {
		    stream << " " << unitIter->second;
		}
	    } else {
		stream << "nicht verfügbar";
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
		case EmsValue::Wartungsmeldungen: map = &MAINTENANCEMESSAGESMAPPING; break;
		case EmsValue::WartungFaellig: map = &MAINTENANCENEEDEDMAPPING; break;
		case EmsValue::Betriebsart: map = &OPMODEMAPPING; break;
		case EmsValue::DesinfektionTag: map = &WEEKDAYMAPPING; break;
		case EmsValue::GebaeudeArt: map = &BUILDINGTYPEMAPPING; break;
		case EmsValue::HeizSystem: map = &HEATINGTYPEMAPPING; break;
		case EmsValue::RegelungsArt: map = &CONTROLTYPEMAPPING; break;
		case EmsValue::FBTyp: map = &REMOTETYPEMAPPING; break;
		case EmsValue::Frostschutz: map = &FROSTPROTECTMAPPING; break;
		case EmsValue::FuehrungsGroesse: map = &RELEVANTVALUEMAPPING; break;
		case EmsValue::UrlaubAbsenkungsArt: map = &VACATIONREDUCTIONMAPPING; break;
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
		f % BE16_TO_CPU(record.code_be16) % BE16_TO_CPU(record.durationMinutes_be16);
		stream << f;
		if (record.time.valid) {
		    boost::format ft("%d.%d.%d %d:%02d");
		    ft % (unsigned int) record.time.day % (unsigned int) record.time.month;
		    ft % (2000 + record.time.year);
		    ft % (unsigned int) record.time.hour % (unsigned int) record.time.minute;
		    stream << "; Zeitpunkt " << ft;
		}
		stream << std::endl;
	    }
	    break;
	}
	case EmsValue::Date: {
	    EmsProto::DateRecord record = value.getValue<EmsProto::DateRecord>();

	    stream << boost::format("%d.%d.%d")
		    % (unsigned int) record.day % (unsigned int) record.month
		    % (2000 + record.year);
	    break;
	}
	case EmsValue::SystemTime: {
	    EmsProto::SystemTimeRecord record = value.getValue<EmsProto::SystemTimeRecord>();
	    auto dayIter = WEEKDAYMAPPING.find(record.dayOfWeek);

	    stream << boost::format("%d.%d.%d")
		    % (unsigned int) record.common.day % (unsigned int) record.common.month
		    % (2000 + record.common.year);

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
    m_cache.handleValue(value);
}
