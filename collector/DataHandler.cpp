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

#include <boost/format.hpp>
#include "DataHandler.h"
#include "CommandHandler.h"

DataHandler::DataHandler(TcpHandler& handler,
			 boost::asio::ip::tcp::endpoint& endpoint) :
    m_handler(handler),
    m_acceptor(handler, endpoint)
{
    startAccepting();
}

DataHandler::~DataHandler()
{
    m_acceptor.close();
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&DataConnection::close, _1));
    m_connections.clear();
}

void
DataHandler::handleAccept(DataConnection::Ptr connection,
			  const boost::system::error_code& error)
{
    if (error) {
	if (error != boost::asio::error::operation_aborted) {
	    std::cerr << "Accept error: " << error.message() << std::endl;
	}
	return;
    }

    startConnection(connection);
    startAccepting();
}

void
DataHandler::startConnection(DataConnection::Ptr connection)
{
    m_connections.insert(connection);
}

void
DataHandler::stopConnection(DataConnection::Ptr connection)
{
    m_connections.erase(connection);
    connection->close();
}

void
DataHandler::handleValue(const EmsValue& value)
{
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&DataConnection::handleValue, _1, value));
}

void
DataHandler::startAccepting()
{
    DataConnection::Ptr connection(new DataConnection(*this));
    m_acceptor.async_accept(connection->socket(),
		            boost::bind(&DataHandler::handleAccept, this,
					connection, boost::asio::placeholders::error));
}


DataConnection::DataConnection(DataHandler& handler) :
    m_socket(handler.getHandler()),
    m_handler(handler)
{
}

DataConnection::~DataConnection()
{
}

void
DataConnection::handleWrite(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	m_handler.stopConnection(shared_from_this());
    }
}

void
DataConnection::handleValue(const EmsValue& value)
{
    static const std::map<EmsValue::Type, const char *> TYPEMAPPING = {
	{ EmsValue::SollTemp, "targettemperature" },
	{ EmsValue::IstTemp, "currenttemperature" },
	{ EmsValue::SetTemp, "settemperature" },
	{ EmsValue::MaxTemp, "maxtemperature" },
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
	{ EmsValue::HeizZeit, "heatingminutes" },
	{ EmsValue::WarmwasserbereitungsZeit, "warmwaterminutes" },
	{ EmsValue::Brennerstarts, "heaterstarts" },
	{ EmsValue::WarmwasserBereitungen, "warmwaterpreparations" },
	{ EmsValue::EinschaltoptimierungsZeit, "onoptimizationminutes" },
	{ EmsValue::AusschaltoptimierungsZeit, "offoptimizationminutes" },
	{ EmsValue::EinschaltHysterese, "onhysteresis" },
	{ EmsValue::AusschaltHysterese, "offhysteresis" },
	{ EmsValue::AntipendelZeit, "antipendelminutes" },
	{ EmsValue::NachlaufZeit, "followupminutes" },
	{ EmsValue::DesinfektionStunde, "desinfectionhour" },
        { EmsValue::HektoStundenVorWartung, "maintenanceintervalin100hours" },
        
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
	{ EmsValue::Ferien, "vacationmode" },
	{ EmsValue::Party, "partymode" },
	{ EmsValue::Frostschutz, "frostsafemode" },
	{ EmsValue::SchaltuhrEin, "switchpointactive" },
	{ EmsValue::KesselSchalter, "masterswitch" },
	{ EmsValue::EigenesProgrammAktiv, "ownschedule" },
	{ EmsValue::EinmalLadungsLED, "onetimeloadindicator" },

	{ EmsValue::WWSystemType, "warmwatersystemtype" },
	{ EmsValue::Schaltpunkte, "switchpoints" },
	{ EmsValue::Wartungsmeldungen, "maintenancereminder" },
	{ EmsValue::WartungFaellig, "maintenancedue" },
	{ EmsValue::Betriebsart, "opmode" },
	{ EmsValue::DesinfektionTag, "desinfectionday" },

	{ EmsValue::HKKennlinie, "characteristic" },
	{ EmsValue::Fehler, "error" },
	{ EmsValue::SystemZeit, "systemtime" },
	{ EmsValue::Wartungstermin, "maintenancedate" },

	{ EmsValue::ServiceCode, "servicecode" },
	{ EmsValue::FehlerCode, "errorcode" }
    };
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
    };
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
	{ 0x10, "B" }, { 0x11, "L" }, { 0x12, "S" }, { 0x13, "S" }
    };

    static const std::map<uint8_t, const char *> OPMODEMAPPING = {
	{ 0, "off" }, { 1, "on" }, { 2, "auto" }
    };

    static const std::map<uint8_t, const char *> DAYMAPPING = {
	{ 0, "monday" }, { 1, "tuesday" }, { 2, "wednesday"}, { 3, "thursday" },
	{ 4, "friday" }, { 5, "saturday" }, { 6, "sunday" }, { 7, "everyday" }
    };

    std::ostringstream stream;
    auto typeIter = TYPEMAPPING.find(value.getType());
    const char *type = typeIter != TYPEMAPPING.end() ? typeIter->second : NULL;
    auto subtypeIter = SUBTYPEMAPPING.find(value.getSubType());
    const char *subtype = subtypeIter != SUBTYPEMAPPING.end() ? subtypeIter->second : NULL;

    if (!type) {
	return;
    }

    if (subtype) {
	stream << subtype << " ";
    }
    stream << type << " ";

    switch (value.getReadingType()) {
	case EmsValue::Numeric:
	    stream << value.getValue<float>();
	    break;
	case EmsValue::Integer:
	    stream << value.getValue<unsigned int>();
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

    output(stream.str());
}
