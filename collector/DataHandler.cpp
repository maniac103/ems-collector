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

#include "DataHandler.h"

DataHandler::DataHandler(TcpHandler& handler,
			 boost::asio::ip::tcp::endpoint& endpoint) :
    m_handler(handler),
    m_acceptor(handler, endpoint)
{
    startAccepting();
}

DataHandler::~DataHandler()
{
    m_acceptor.close();
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&DataConnection::close, _1));
    m_connections.clear();
}

void
DataHandler::handleAccept(DataConnection::Ptr connection,
			  const boost::system::error_code& error)
{
    if (error) {
	if (error != boost::asio::error::operation_aborted) {
	    std::cerr << "Accept error: " << error.message() << std::endl;
	}
	return;
    }

    startConnection(connection);
    startAccepting();
}

void
DataHandler::startConnection(DataConnection::Ptr connection)
{
    m_connections.insert(connection);
}

void
DataHandler::stopConnection(DataConnection::Ptr connection)
{
    m_connections.erase(connection);
    connection->close();
}

void
DataHandler::handleValue(const EmsValue& value)
{
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&DataConnection::handleValue, _1, value));
}

void
DataHandler::startAccepting()
{
    DataConnection::Ptr connection(new DataConnection(*this));
    m_acceptor.async_accept(connection->socket(),
		            boost::bind(&DataHandler::handleAccept, this,
					connection, boost::asio::placeholders::error));
}


DataConnection::DataConnection(DataHandler& handler) :
    m_socket(handler.getHandler()),
    m_handler(handler)
{
}

DataConnection::~DataConnection()
{
}

void
DataConnection::handleWrite(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	m_handler.stopConnection(shared_from_this());
    }
}

void
DataConnection::handleValue(const EmsValue& value)
{
    std::ostringstream ss;
    ss << "implement me: got value " << value.getType();
    output(ss.str());
}
