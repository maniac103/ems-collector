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

#ifndef __MQTTADAPTER_H__
#define __MQTTADAPTER_H__

#include "CommandScheduler.h"
#include "EmsMessage.h"

#ifdef HAVE_MQTT

#include <mqtt_client_cpp.hpp>
#include "ApiCommandParser.h"
#include "Noncopyable.h"

class MqttAdapter : public boost::noncopyable
{
    public:
	MqttAdapter(boost::asio::io_service& ios,
		    EmsCommandSender *sender,
		    const std::string& host, const std::string& port,
		    const std::string& topicPrefix);

	void handleValue(const EmsValue& value);

    private:
	bool onConnect(bool sessionPresent, mqtt::connect_return_code returnCode);
	void onError(const mqtt::error_code& ec);
	void onClose();
	bool onMessageReceived(const mqtt::buffer& topic, const mqtt::buffer& contents);
	void scheduleConnectionRetry();

    private:
	class CommandClient : public EmsCommandClient {
	    public:
		CommandClient(MqttAdapter *adapter) :
		    m_adapter(adapter)
		{ }
		virtual void onIncomingMessage(const EmsMessage& message) override {
		    m_adapter->m_commandParser->onIncomingMessage(message);
		}
		virtual void onTimeout() override {
		    m_adapter->m_commandParser->onTimeout();
		}
	    private:
		MqttAdapter *m_adapter;
	};
	friend class CommandClient;

    private:
	static const unsigned int MinRetryDelaySeconds = 5;
	static const unsigned int MaxRetryDelaySeconds = 5 * 60;

	std::shared_ptr<mqtt::callable_overlay<mqtt::client<
		mqtt::tcp_endpoint<boost::asio::ip::tcp::socket, boost::asio::io_service::strand> > > > m_client;
	EmsCommandSender * m_sender;
	boost::shared_ptr<EmsCommandClient> m_cmdClient;
	bool m_connected;
	unsigned int m_retryDelay;
	std::unique_ptr<ApiCommandParser> m_commandParser;
	boost::asio::deadline_timer m_retryTimer;
	std::string m_topicPrefix;
};

#else /* HAVE_MQTT */

class MqttAdapter {
    public:
	MqttAdapter(boost::asio::io_service& /* ios */,
		    EmsCommandSender * /* sender */,
		    const std::string& /* host */, const std::string& /* port */,
		    const std::string& /* topicPrefix */)
	{}

	void handleValue(const EmsValue& /* value */) {}
};

#endif /* !HAVE_MQTT */

#endif /* __MQTTADAPTER_H__ */
