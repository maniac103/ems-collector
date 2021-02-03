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

#include <algorithm>
#include <boost/bind/bind.hpp>
#include "MqttAdapter.h"
#include "Options.h"
#include "ValueApi.h"

MqttAdapter::MqttAdapter(boost::asio::io_service& ios,
			 EmsCommandSender *sender,
			 const std::string& host, const std::string& port,
			 const std::string& topicPrefix) :
    m_client(mqtt::make_client(ios, host, port)),
    m_sender(sender),
    m_cmdClient(new CommandClient(this)),
    m_connected(false),
    m_retryDelay(MinRetryDelaySeconds),
    m_retryTimer(ios),
    m_topicPrefix(topicPrefix.empty() ? "/ems" : topicPrefix)
{
    m_client->set_client_id("ems-collector");
    m_client->set_error_handler(boost::bind(&MqttAdapter::onError, this, boost::placeholders::_1));
    m_client->set_connack_handler(boost::bind(&MqttAdapter::onConnect, this,
					      boost::placeholders::_1, boost::placeholders::_2));
    m_client->set_close_handler(boost::bind(&MqttAdapter::onClose, this));
    m_client->set_publish_handler(boost::bind(&MqttAdapter::onMessageReceived, this,
					      boost::placeholders::_3, boost::placeholders::_4));
    m_client->connect();
}

void
MqttAdapter::handleValue(const EmsValue& value)
{
    if (!m_connected) {
	return;
    }

    std::string type = ValueApi::getTypeName(value.getType());
    std::string subtype = ValueApi::getSubTypeName(value.getSubType());

    std::string topic = m_topicPrefix + "/sensor/";
    if (!subtype.empty()) {
	topic += subtype + "/";
    }
    if (!type.empty()) {
	topic += type + "/";
    }
    topic += "value";

    std::string formattedValue = ValueApi::formatValue(value);
    DebugStream& debug = Options::ioDebug();
    if (debug) {
	debug << "MQTT: publishing topic '" << topic << "' with value " << formattedValue << std::endl;
    }
    m_client->publish(topic, formattedValue, mqtt::qos::at_most_once);
}

bool
MqttAdapter::onConnect(bool sessionPresent, mqtt::connect_return_code returnCode)
{
    Options::ioDebug() << "MQTT: onConnect, return code "
		       << std::dec << (unsigned int) returnCode << std::endl;
    m_connected = returnCode == mqtt::connect_return_code::accepted;
    if (!m_connected) {
	m_retryDelay = MinRetryDelaySeconds;
	scheduleConnectionRetry();
    } else if (m_sender) {
	m_client->subscribe(m_topicPrefix + "/control/#", mqtt::qos::exactly_once);
	auto outputCb = [] (const std::string&) {};
	m_commandParser.reset(
		new ApiCommandParser(*m_sender, m_cmdClient, nullptr, outputCb));
    }
    return true;
}

void
MqttAdapter::onError(const mqtt::error_code& ec)
{
    Options::ioDebug() << "MQTT: onError, code " << std::dec << ec << std::endl;
    m_connected = false;
    m_commandParser.reset();
    scheduleConnectionRetry();
}

void
MqttAdapter::onClose()
{
    Options::ioDebug() << "MQTT: onClose" << std::endl;
    m_connected = false;
    m_commandParser.reset();
    m_retryDelay = MinRetryDelaySeconds;
    scheduleConnectionRetry();
}

bool
MqttAdapter::onMessageReceived(const mqtt::buffer& topic, const mqtt::buffer& contents)
{
    Options::ioDebug() << "MQTT: got incoming message, topic " << topic << ", contents " << contents << std::endl;
    std::string command = topic.substr(m_topicPrefix.length() + 9).to_string(); // strip '/ems/control/'
    std::replace(command.begin(), command.end(), '/', ' ');
    std::istringstream commandStream(command + " " + contents.to_string());
    m_commandParser->parse(commandStream);
    return true;
}

void
MqttAdapter::scheduleConnectionRetry()
{
    Options::ioDebug() << "MQTT: scheduling reconnection in " << std::dec << m_retryDelay << "s" << std::endl;
    m_retryTimer.expires_from_now(boost::posix_time::seconds(m_retryDelay));
    m_retryTimer.async_wait([this] (const boost::system::error_code& error) {
	if (error != boost::asio::error::operation_aborted) {
	    m_retryDelay = std::min(m_retryDelay * 2, MaxRetryDelaySeconds);
	    m_client->connect();
	}
    });
}

