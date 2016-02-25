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

#ifdef HAVE_MQTT

#include <mqtt_client_cpp.hpp>
#include "EmsMessage.h"

class MqttAdapter {
    public:
	MqttAdapter(boost::asio::io_service& ios,
		    const std::string& host, const std::string& port);
	~MqttAdapter();

	void handleValue(const EmsValue& value);

    private:
	void onConnect(bool sessionPresent, uint8_t returnCode);
	void onError(const boost::system::error_code& ec);

    private:
	mqtt::client<boost::asio::ip::tcp::socket> m_client;
};

#else /* HAVE_MQTT */

class MqttAdapter {
    public:
	MqttAdapter(boost::asio::io_service& /* ios */,
		    const std::string& /* host */, const std::string& /* port */)
	{}

	void handleValue(const EmsValue& /* value */) {}
};

#endif /* !HAVE_MQTT */

#endif /* __MQTTADAPTER_H__ */
