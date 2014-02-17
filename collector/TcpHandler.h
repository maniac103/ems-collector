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

#ifndef __TCPHANDLER_H__
#define __TCPHANDLER_H__

#include "IoHandler.h"
#include <boost/shared_ptr.hpp>

class CommandHandler;
class DataHandler;

class TcpHandler : public IoHandler
{
    public:
	TcpHandler(const std::string& host, const std::string& port, Database& db);
	~TcpHandler();
	void sendMessage(const EmsMessage& msg);

    protected:
	virtual void readStart() {
	    /* Start an asynchronous read and call read_complete when it completes or fails */
	    m_socket.async_read_some(boost::asio::buffer(m_recvBuffer, maxReadLength),
				     boost::bind(&TcpHandler::readComplete, this,
						 boost::asio::placeholders::error,
						 boost::asio::placeholders::bytes_transferred));
	}

	virtual void doCloseImpl();
	virtual void readComplete(const boost::system::error_code& error, size_t bytesTransferred);

    private:
	void handleConnect(const boost::system::error_code& error);
	void resetWatchdog();
	void watchdogTimeout(const boost::system::error_code& error);

    private:
	boost::asio::ip::tcp::socket m_socket;
	boost::asio::deadline_timer m_watchdog;
	boost::shared_ptr<CommandHandler> m_cmdHandler;
	boost::shared_ptr<DataHandler> m_dataHandler;
};

#endif /* __TCPHANDLER_H__ */
