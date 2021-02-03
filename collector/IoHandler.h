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

#include <list>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/function.hpp>
#include "EmsMessage.h"
#include "ValueCache.h"

class IoHandler : public boost::asio::io_service
{
    public:
	typedef std::function<void (const EmsValue& value)> ValueCallback;

    public:
	IoHandler(ValueCache& cache);

	void close() {
	    post(boost::bind(&IoHandler::doClose, this,
			     boost::system::error_code()));
	}

	bool active() {
	    return m_active;
	}

	void addValueCallback(ValueCallback& cb) {
	    m_valueCallbacks.push_back(cb);
	}

    protected:
	/* maximum amount of data to read in one operation */
	static const int maxReadLength = 512;

	virtual void readStart() = 0;
	virtual void doCloseImpl() = 0;

	virtual void onPcMessageReceived(const EmsMessage& /* message */) { }
	virtual void readComplete(const boost::system::error_code& error, size_t bytesTransferred);
	void doClose(const boost::system::error_code& error);
	void handleValue(const EmsValue& value);

	bool m_active;
	unsigned char m_recvBuffer[maxReadLength];

    private:
	typedef enum {
	    Syncing,
	    Length,
	    Data,
	    Checksum
	} State;

	State m_state;
	size_t m_pos, m_length;
	uint8_t m_checkSum;
	std::vector<uint8_t> m_data;
	std::list<ValueCallback> m_valueCallbacks;
	EmsMessage::ValueHandler m_valueCb;
	EmsMessage::CacheAccessor m_cacheCb;
};

#endif /* __IOHANDLER_H__ */
