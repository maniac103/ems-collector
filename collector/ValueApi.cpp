/*
 * Buderus EMS data collector
 *
 * Copyright (C) 2014 Danny Baumann <dannybaumann@web.de>
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

#include <map>
#include <sstream>
#include <boost/format.hpp>
#include "CommandHandler.h"
#include "ValueApi.h"

std::string
ValueApi::getTypeName(EmsValue::Type type)
{
    static const std::map<EmsValue::Type, const char *> TYPEMAPPING = {
	{ EmsValue::SollTemp, "targettemperature" },
	{ EmsValue::IstTemp, "currenttemperature" },
	{ EmsValue::SetTemp, "settemperature" },
	{ EmsValue::MinTemp, "mintemperature" },
	{ EmsValue::MaxTemp, "maxtemperature" },
	{ EmsValue::TagTemp, "daytemperature" },
	{ EmsValue::NachtTemp, "nighttemperature" },
	{ EmsValue::UrlaubTemp, "vacationtemperature" },
	{ EmsValue::RaumEinfluss, "maxroomeffect" },
	{ EmsValue::RaumOffset, "roomoffset" },
	{ EmsValue::SchwelleSommerWinter, "summerwinterthreshold" },
	{ EmsValue::FrostSchutzTemp, "frostsafetemperature" },
	{ EmsValue::AuslegungsTemp, "designtemperature" },
	{ EmsValue::RaumUebersteuerTemp, "temperatureoverride" },
	{ EmsValue::AbsenkungsSchwellenTemp, "reducedmodethreshold" },
	{ EmsValue::UrlaubAbsenkungsSchwellenTemp, "vacationreducedmodethreshold" },
	{ EmsValue::AbsenkungsAbbruchTemp, "cancelreducedmodethreshold" },
	{ EmsValue::GedaempfteTemp, "dampedtemperature" },
	{ EmsValue::DesinfektionsTemp, "desinfectiontemperature" },
	{ EmsValue::TemperaturAenderung, "temperaturechange" },
	{ EmsValue::Mischersteuerung, "mixercontrol" },
	{ EmsValue::Flammenstrom, "flamecurrent" },
	{ EmsValue::Systemdruck, "pressure" },
	{ EmsValue::BetriebsZeit, "operatingminutes" },
	{ EmsValue::MinModulation, "minmodulation" },
	{ EmsValue::MaxModulation, "maxmodulation" },
	{ EmsValue::SollModulation, "targetmodulation" },
	{ EmsValue::IstModulation, "currentmodulation" },
	{ EmsValue::SollLeistung, "requestedpower" },
	{ EmsValue::EinschaltHysterese, "onhysteresis" },
	{ EmsValue::AusschaltHysterese, "offhysteresis" },
	{ EmsValue::DurchflussMenge, "flowrate" },

	{ EmsValue::HeizZeit, "heatingminutes" },
	{ EmsValue::WarmwasserbereitungsZeit, "warmwaterminutes" },
	{ EmsValue::Brennerstarts, "heaterstarts" },
	{ EmsValue::WarmwasserBereitungen, "warmwaterpreparations" },
	{ EmsValue::EinschaltoptimierungsZeit, "onoptimizationminutes" },
	{ EmsValue::AusschaltoptimierungsZeit, "offoptimizationminutes" },
	{ EmsValue::AntipendelZeit, "antipendelminutes" },
	{ EmsValue::NachlaufZeit, "followupminutes" },
	{ EmsValue::DesinfektionStunde, "desinfectionhour" },
	{ EmsValue::HektoStundenVorWartung, "maintenanceintervalin100hours" },
	{ EmsValue::PausenZeit, "pausehours" },
	{ EmsValue::PartyZeit, "partyhours" },

	{ EmsValue::FlammeAktiv, "flameactive" },
	{ EmsValue::BrennerAktiv, "heateractive" },
	{ EmsValue::ZuendungAktiv, "ignitionactive" },
	{ EmsValue::PumpeAktiv, "pumpactive" },
	{ EmsValue::ZirkulationAktiv, "zirkpumpactive" },
	{ EmsValue::DreiWegeVentilAufWW, "3wayonww" },
	{ EmsValue::EinmalLadungAktiv, "onetimeload" },
	{ EmsValue::DesinfektionAktiv, "desinfectionactive" },
	{ EmsValue::Desinfektion, "desinfection" },
	{ EmsValue::NachladungAktiv, "boostcharge" },
	{ EmsValue::WarmwasserBereitung, "warmwaterpreparationactive" },
	{ EmsValue::WarmwasserTempOK, "warmwatertempok" },
	{ EmsValue::Automatikbetrieb, "automode" },
	{ EmsValue::Tagbetrieb, "daymode" },
	{ EmsValue::Sommerbetrieb, "summermode" },
	{ EmsValue::Ausschaltoptimierung, "offoptimization" },
	{ EmsValue::Einschaltoptimierung, "onoptimization" },
	{ EmsValue::Estrichtrocknung, "floordrying" },
	{ EmsValue::WWVorrang, "wwoverride" },
	{ EmsValue::Urlaub, "vacationmode" },
	{ EmsValue::Ferien, "holidaymode" },
	{ EmsValue::Party, "partymode" },
	{ EmsValue::Pause, "pausemode" },
	{ EmsValue::Frostschutzbetrieb, "frostsafemodeactive" },
	{ EmsValue::SchaltuhrEin, "switchpointactive" },
	{ EmsValue::KesselSchalter, "masterswitch" },
	{ EmsValue::EigenesProgrammAktiv, "customschedule" },
	{ EmsValue::EinmalLadungsLED, "onetimeloadindicator" },
	{ EmsValue::ATDaempfung, "damping" },
	{ EmsValue::SchaltzeitOptimierung, "scheduleoptimizer" },
	{ EmsValue::Fuehler1Defekt, "sensor1failure" },
	{ EmsValue::Fuehler2Defekt, "sensor2failure" },
	{ EmsValue::Manuellbetrieb, "manualmode" },
	{ EmsValue::Stoerung, "failure" },
	{ EmsValue::StoerungDesinfektion, "desinfectionfailure" },
	{ EmsValue::Ladevorgang, "loading" },
	{ EmsValue::WWSystemType, "warmwatersystemtype" },
	{ EmsValue::Schaltpunkte, "switchpoints" },
	{ EmsValue::Wartungsmeldungen, "maintenancereminder" },
	{ EmsValue::WartungFaellig, "maintenancedue" },
	{ EmsValue::Betriebsart, "opmode" },
	{ EmsValue::DesinfektionTag, "desinfectionday" },
	{ EmsValue::GebaeudeArt, "buildingtype" },
	{ EmsValue::RegelungsArt, "controltype" },
	{ EmsValue::HeizSystem, "heatsystem" },
	{ EmsValue::FuehrungsGroesse, "relevantparameter" },
	{ EmsValue::Frostschutz, "frostsafemode" },
	{ EmsValue::UrlaubAbsenkungsArt, "vacationreductionmode" },
	{ EmsValue::FBTyp, "remotecontroltype" },

	{ EmsValue::HKKennlinie, "characteristic" },
	{ EmsValue::Fehler, "error" },
	{ EmsValue::SystemZeit, "systemtime" },
	{ EmsValue::Wartungstermin, "maintenancedate" },

	{ EmsValue::ServiceCode, "servicecode" },
	{ EmsValue::FehlerCode, "errorcode" }
    };

    auto iter = TYPEMAPPING.find(type);
    if (iter == TYPEMAPPING.end()) {
	return "";
    }

    return iter->second;
}

std::string
ValueApi::getSubTypeName(EmsValue::SubType subtype)
{
    static const std::map<EmsValue::SubType, const char *> SUBTYPEMAPPING = {
	{ EmsValue::HK1, "hk1" },
	{ EmsValue::HK2, "hk2" },
	{ EmsValue::HK3, "hk3" },
	{ EmsValue::HK4, "hk4" },
	{ EmsValue::Kessel, "heater" },
	{ EmsValue::KesselPumpe, "heaterpump" },
	{ EmsValue::Brenner, "burner" },
	{ EmsValue::Ruecklauf, "returnflow" },
	{ EmsValue::Waermetauscher, "heatexchanger" },
	{ EmsValue::WW, "ww" },
	{ EmsValue::Zirkulation, "zirkpump" },
	{ EmsValue::Raum, "indoor" },
	{ EmsValue::Aussen, "outdoor" },
	{ EmsValue::Abgas, "exhaust" },
	{ EmsValue::Ansaugluft, "intake" }
    };

    auto iter = SUBTYPEMAPPING.find(subtype);
    if (iter == SUBTYPEMAPPING.end()) {
	return "";
    }

    return iter->second;
}

std::string
ValueApi::formatValue(const EmsValue& value)
{
    static const std::map<uint8_t, const char *> WWSYSTEMMAPPING = {
	{ EmsProto::WWSystemNone, "none" },
	{ EmsProto::WWSystemDurchlauf, "tankless" },
	{ EmsProto::WWSystemKlein, "small" },
	{ EmsProto::WWSystemGross, "large" },
	{ EmsProto::WWSystemSpeicherlade, "speicherladesystem" }
    };

    static const std::map<uint8_t, const char *> ZIRKSPMAPPING = {
	{ 0, "off" }, { 1, "1x" }, { 2, "2x" }, { 3, "3x" },
	{ 4, "4x" }, { 5, "5x" }, { 6, "6x" }, { 7, "alwayson" }
    };

    static const std::map<uint8_t, const char *> MAINTENANCEMESSAGESMAPPING = {
	{ 0, "off" }, { 1, "byhours" }, { 2, "bydate" }
    };

    static const std::map<uint8_t, const char *> MAINTENANCENEEDEDMAPPING = {
	{ 0, "no" }, { 3, "byhours" }, { 8, "bydate" }
    };

    static const std::map<uint8_t, const char *> ERRORTYPEMAPPING = {
	{ 0x10, "L" }, { 0x11, "B" }, { 0x12, "S" }, { 0x13, "D" }
    };

    static const std::map<uint8_t, const char *> OPMODEMAPPING = {
	{ 0, "off" }, { 1, "on" }, { 2, "auto" }
    };

    static const std::map<uint8_t, const char *> DAYMAPPING = {
	{ 0, "monday" }, { 1, "tuesday" }, { 2, "wednesday"}, { 3, "thursday" },
	{ 4, "friday" }, { 5, "saturday" }, { 6, "sunday" }, { 7, "everyday" }
    };

    static const std::map<uint8_t, const char *> BUILDINGTYPEMAPPING = {
	{ 0, "light" }, { 1, "medium" }, { 2, "heavy" }
    };

    static const std::map<uint8_t, const char *> HEATINGTYPEMAPPING = {
	{ 1, "heater" }, { 2, "convector" }, { 3, "floorheater" },
    };

    static const std::map<uint8_t, const char *> CONTROLTYPEMAPPING = {
	{ 0, "offmode" }, { 1, "reduced" }, { 2, "raumhalt" }, { 3, "aussenhalt" }
    };

    static const std::map<uint8_t, const char *> FROSTPROTECTMAPPING = {
	{ 0, "off" }, { 1, "byoutdoortemp" }, { 2, "byindoortemp" }
    };

    static const std::map<uint8_t, const char *> RELEVANTVALUEMAPPING = {
	{ 0, "outdoor" }, { 1, "indoor" }
    };

    static const std::map<uint8_t, const char *> VACATIONREDUCTIONMAPPING = {
	{ 3, "outdoor" }, { 2, "indoor" }
    };

    static const std::map<uint8_t, const char *> REMOTETYPEMAPPING = {
	{ 0, "none" }, { 1, "rc20" }, { 2, "rc3x" }
    };

    std::ostringstream stream;

    switch (value.getReadingType()) {
	case EmsValue::Numeric:
	    if (!value.isValid()) {
		stream << "unavailable";
	    } else {
		stream << value.getValue<float>();
	    }
	    break;
	case EmsValue::Integer:
	    if (!value.isValid()) {
		stream << "unavailable";
	    } else {
		stream << value.getValue<unsigned int>();
	    }
	    break;
	case EmsValue::Boolean:
	    stream << (value.getValue<bool>() ? "on" : "off");
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
		case EmsValue::DesinfektionTag: map = &DAYMAPPING; break;
		case EmsValue::GebaeudeArt: map = &BUILDINGTYPEMAPPING; break;
		case EmsValue::HeizSystem: map = &HEATINGTYPEMAPPING; break;
		case EmsValue::RegelungsArt: map = &CONTROLTYPEMAPPING; break;
		case EmsValue::Frostschutz: map = &FROSTPROTECTMAPPING; break;
		case EmsValue::FuehrungsGroesse: map = &RELEVANTVALUEMAPPING; break;
		case EmsValue::FBTyp: map = &REMOTETYPEMAPPING; break;
		case EmsValue::UrlaubAbsenkungsArt: map = &VACATIONREDUCTIONMAPPING; break;
		default: break;
	    }
	    if (map && map->find(enumValue) != map->end()) {
		stream << map->at(enumValue);
	    } else {
		stream << (unsigned int) enumValue;
	    }
	    break;
	}
	case EmsValue::Kennlinie: {
	    std::vector<uint8_t> kennlinie = value.getValue<std::vector<uint8_t> >();
	    stream << boost::format("%d/%d/%d")
		    % (unsigned int) kennlinie[0] % (unsigned int) kennlinie[1]
		    % (unsigned int) kennlinie[2];
	    break;
	}
	case EmsValue::Error: {
	    EmsValue::ErrorEntry entry = value.getValue<EmsValue::ErrorEntry>();
	    std::string formatted = CommandConnection::buildRecordResponse(&entry.record);
	    if (formatted.empty()) {
		formatted = "empty";
	    }

	    stream << boost::format("%s%02d %s")
		    % ERRORTYPEMAPPING.at(entry.type) % entry.index % formatted;
	    break;
	}
	case EmsValue::Date: {
	    EmsProto::DateRecord record = value.getValue<EmsProto::DateRecord>();
	    stream << boost::format("%04d-%02d-%02d")
		    % (2000 + record.year) % (unsigned int) record.month
		    % (unsigned int) record.day;
	    break;
	}
	case EmsValue::SystemTime: {
	    EmsProto::SystemTimeRecord record = value.getValue<EmsProto::SystemTimeRecord>();
	    stream << boost::format("%04d-%02d-%02d %02d:%02d:%02d")
		    % (2000 + record.common.year) % (unsigned int) record.common.month
		    % (unsigned int) record.common.day % (unsigned int) record.common.hour
		    % (unsigned int)  record.common.minute % (unsigned int) record.second;
	    break;
	}
	case EmsValue::Formatted:
	    stream << value.getValue<std::string>();
	    break;
    }

    return stream.str();
}
