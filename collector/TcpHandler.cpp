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
#include "TcpHandler.h"
#include "CommandHandler.h"

TcpHandler::TcpHandler(const std::string& host,
		       const std::string& port,
		       Database& db) :
    IoHandler(db),
    m_socket(*this),
    m_watchdog(*this)
{
    boost::system::error_code error;
    boost::asio::ip::tcp::resolver resolver(*this);
    boost::asio::ip::tcp::resolver::query query(host, port);
    boost::asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(query, error);

    if (error) {
	doClose(error);
    } else {
	m_socket.async_connect(*endpoint,
			       boost::bind(&TcpHandler::handleConnect, this,
					   boost::asio::placeholders::error));
    }
}

TcpHandler::~TcpHandler()
{
    if (m_active) {
	m_socket.close();
    }
}

void
TcpHandler::handleConnect(const boost::system::error_code& error)
{
    if (error) {
	doClose(error);
    } else {
	boost::asio::ip::tcp::endpoint cmdEndpoint(boost::asio::ip::tcp::v4(), 7777);
	m_cmdHandler.reset(new CommandHandler(*this, cmdEndpoint));
	m_pcMessageCallback = boost::bind(&CommandHandler::handlePcMessage,
					  m_cmdHandler, _1);
	resetWatchdog();
	readStart();
    }
}

void
TcpHandler::resetWatchdog()
{
    m_watchdog.expires_from_now(boost::posix_time::minutes(2));
    m_watchdog.async_wait(boost::bind(&TcpHandler::watchdogTimeout, this,
				      boost::asio::placeholders::error));
}

void
TcpHandler::watchdogTimeout(const boost::system::error_code& error)
{
    if (error != boost::asio::error::operation_aborted) {
	doClose(error);
    }
}

void
TcpHandler::readComplete(const boost::system::error_code& error, size_t bytesTransferred)
{
    resetWatchdog();
    IoHandler::readComplete(error, bytesTransferred);
}

void
TcpHandler::doCloseImpl()
{
    m_watchdog.cancel();
    m_cmdHandler.reset();
    m_pcMessageCallback.clear();
    m_socket.close();
}

void
TcpHandler::sendMessage(const EmsMessage& msg)
{
    boost::system::error_code error;

    boost::asio::write(m_socket, boost::asio::buffer(msg.getSendData()),
		       boost::asio::transfer_all(), error);
}

