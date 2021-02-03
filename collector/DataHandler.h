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

#ifndef __DATAHANDLER_H__
#define __DATAHANDLER_H__

#include <set>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include "EmsMessage.h"
#include "Noncopyable.h"

class DataHandler;

class DataConnection : public boost::enable_shared_from_this<DataConnection>,
		       private boost::noncopyable
{
    public:
	typedef boost::shared_ptr<DataConnection> Ptr;

    public:
	DataConnection(boost::asio::io_service& ios, DataHandler& handler);
	~DataConnection();

    public:
	boost::asio::ip::tcp::socket& socket() {
	    return m_socket;
	}
	void close() {
	    m_socket.close();
	}
	void handleValue(const EmsValue& value);

    private:
	void handleWrite(const boost::system::error_code& error);

	void output(const std::string& text) {
	    boost::asio::async_write(m_socket, boost::asio::buffer(text + "\n"),
		boost::bind(&DataConnection::handleWrite, shared_from_this(),
			    boost::asio::placeholders::error));
	}
    private:
	boost::asio::ip::tcp::socket m_socket;
	DataHandler& m_handler;
};

class DataHandler : private boost::noncopyable
{
    public:
	DataHandler(boost::asio::io_service& ios,
		    boost::asio::ip::tcp::endpoint& endpoint);
	~DataHandler();

    public:
	void startConnection(DataConnection::Ptr connection);
	void stopConnection(DataConnection::Ptr connection);
	void handleValue(const EmsValue& value);

    private:
	void handleAccept(DataConnection::Ptr connection,
			  const boost::system::error_code& error);
	void startAccepting();

    private:
	boost::asio::io_service& m_ios;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::set<DataConnection::Ptr> m_connections;
};

#endif /* __DATAHANDLER_H__ */
