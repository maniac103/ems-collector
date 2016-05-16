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
#include "Options.h"

TcpHandler::TcpHandler(const std::string& host,
		       const std::string& port,
		       ValueCache& cache) :
    IoHandler(cache),
    EmsCommandSender((boost::asio::io_service&) *this),
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
	m_socket.async_connect(*endpoint, [this] (const boost::system::error_code& error) {
	    if (error) {
		doClose(error);
	    } else {
		resetWatchdog();
		readStart();
	    }
	});
    }
}

TcpHandler::~TcpHandler()
{
    if (m_active) {
	m_socket.close();
    }
}

void
TcpHandler::resetWatchdog()
{
    m_watchdog.expires_from_now(boost::posix_time::minutes(2));
    m_watchdog.async_wait([this] (const boost::system::error_code& error) {
	if (error != boost::asio::error::operation_aborted) {
	    doClose(error);
	}
    });
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
    m_socket.close();
}

void
TcpHandler::sendMessageImpl(const EmsMessage& msg)
{
    boost::system::error_code error;
    std::vector<uint8_t> sendData = msg.getSendData(true);
    DebugStream& debug = Options::ioDebug();

    if (debug) {
	debug << "IO: Sending bytes ";
	for (size_t i = 0; i < sendData.size(); i++) {
	    debug << std::setfill('0') << std::setw(2)
		  << std::showbase << std::hex
		  << (unsigned int) sendData[i] << " ";
	}
	debug << std::endl;
    }

    boost::asio::write(m_socket, boost::asio::buffer(sendData),
		       boost::asio::transfer_all(), error);
}

