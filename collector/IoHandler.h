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

#ifndef __IOHANDLER_H__
#define __IOHANDLER_H__

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <fstream>
#include "Database.h"
#include "EmsMessage.h"
#include "MqttAdapter.h"
#include "ValueCache.h"

class IoHandler : public boost::asio::io_service
{
    public:
	IoHandler(Database& db, ValueCache& cache);
	~IoHandler();

	void close() {
	    post(boost::bind(&IoHandler::doClose, this,
			     boost::system::error_code()));
	}

	void setMqttAdapter(MqttAdapter *adapter) {
	    m_mqttAdapter.reset(adapter);
	}

	bool active() {
	    return m_active;
	}
	ValueCache& getCache() {
	    return m_cache;
	}

    protected:
	/* maximum amount of data to read in one operation */
	static const int maxReadLength = 512;

	virtual void readStart() = 0;
	virtual void doCloseImpl() = 0;

	virtual void readComplete(const boost::system::error_code& error, size_t bytesTransferred);
	void doClose(const boost::system::error_code& error);
	void handleValue(const EmsValue& value);
	const EmsValue * getCacheValue(EmsValue::Type type, EmsValue::SubType subtype) {
	    return m_cache.getValue(type, subtype);
	}

	bool m_active;
	unsigned char m_recvBuffer[maxReadLength];
	boost::function<void (const EmsMessage& message)> m_pcMessageCallback;
	boost::function<void (const EmsValue& value)> m_valueCallback;

    private:
	typedef enum {
	    Syncing,
	    Length,
	    Data,
	    Checksum
	} State;

	Database& m_db;
	ValueCache& m_cache;

	State m_state;
	size_t m_pos, m_length;
	uint8_t m_checkSum;
	std::vector<uint8_t> m_data;
	std::unique_ptr<MqttAdapter> m_mqttAdapter;
	EmsMessage::ValueHandler m_valueCb;
	EmsMessage::CacheAccessor m_cacheCb;
};

#endif /* __IOHANDLER_H__ */
