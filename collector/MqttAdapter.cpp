/*
 * Buderus EMS data collector
 *
 * Copyright (C) 2016 Danny Baumann <dannybaumann@web.de>
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

#include <boost/bind.hpp>
#include "MqttAdapter.h"
#include "ValueApi.h"
#include <iostream>

MqttAdapter::MqttAdapter(boost::asio::io_service& ios,
			 const std::string& host, const std::string& port) :
    m_client(mqtt::make_client(ios, host, port))
{
    m_client.set_client_id("ems-collector");
    m_client.set_error_handler(boost::bind(&MqttAdapter::onError, this, _1));
    m_client.set_connack_handler(boost::bind(&MqttAdapter::onConnect, this, _1, _2));

    m_client.connect();
}

MqttAdapter::~MqttAdapter()
{
    m_client.disconnect();
}

void
MqttAdapter::handleValue(const EmsValue& value)
{
    std::string type = ValueApi::getTypeName(value.getType());
    std::string subtype = ValueApi::getSubTypeName(value.getSubType());

    std::string topic = "/ems/sensor/";
    if (!subtype.empty()) {
	topic += subtype + "/";
    }
    if (!type.empty()) {
	topic += type + "/";
    }
    topic += "value";

    std::cerr << "publish on topic " << topic << std::endl;

    m_client.publish_at_most_once(topic, ValueApi::formatValue(value));
}

void
MqttAdapter::onConnect(bool sessionPresent, uint8_t returnCode)
{
    std::cerr << "onConnect: " << sessionPresent << " - " << returnCode << std::endl;
}

void
MqttAdapter::onError(const boost::system::error_code& ec)
{
    std::cerr << "onError: " << ec << std::endl;
}

