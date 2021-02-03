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

#include <boost/bind/bind.hpp>
#include <iostream>
#include "CommandHandler.h"

CommandHandler::CommandHandler(boost::asio::io_service& ios,
			       EmsCommandSender& sender,
			       ValueCache *cache,
			       boost::asio::ip::tcp::endpoint& endpoint) :
    m_ios(ios),
    m_sender(sender),
    m_cache(cache),
    m_acceptor(ios, endpoint)
{
    startAccepting();
}

CommandHandler::~CommandHandler()
{
    m_acceptor.close();
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&CommandConnection::close, boost::placeholders::_1));
    m_connections.clear();
}

void
CommandHandler::handleAccept(CommandConnection::Ptr connection,
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
CommandHandler::startConnection(CommandConnection::Ptr connection)
{
    m_connections.insert(connection);
    connection->startRead();
}

void
CommandHandler::stopConnection(CommandConnection::Ptr connection)
{
    m_connections.erase(connection);
    connection->close();
}

void
CommandHandler::startAccepting()
{
    CommandConnection::Ptr connection(new CommandConnection(m_ios, m_sender, *this, m_cache));
    m_acceptor.async_accept(connection->socket(),
		            boost::bind(&CommandHandler::handleAccept, this,
					connection, boost::asio::placeholders::error));
}


CommandConnection::CommandConnection(boost::asio::io_service& ios,
				     EmsCommandSender& sender,
				     CommandHandler& handler,
				     ValueCache *cache) :
    m_socket(ios),
    m_commandClient(new CommandClient(this)),
    m_parser(sender, m_commandClient, cache, boost::bind(&CommandConnection::respond, this, boost::placeholders::_1)),
    m_handler(handler)
{
}

void
CommandConnection::handleRequest(const boost::system::error_code& error)
{
    if (error) {
	if (error != boost::asio::error::operation_aborted) {
	    m_handler.stopConnection(shared_from_this());
	}
	return;
    }

    std::istream requestStream(&m_request);
    ApiCommandParser::CommandResult result = m_request.size() > 2
	    ? m_parser.parse(requestStream) : ApiCommandParser::InvalidCmd;

    switch (result) {
	case ApiCommandParser::Ok:
	    break;
	case ApiCommandParser::Busy:
	    respond("ERRBUSY");
	    break;
	case ApiCommandParser::InvalidCmd:
	    respond("ERRCMD");
	    break;
	case ApiCommandParser::InvalidArgs:
	    respond("ERRARGS");
	    break;
    }

    requestStream.clear();
    /* drain remainder */
    std::string remainder;
    std::getline(requestStream, remainder);

    startRead();
}

void
CommandConnection::handleWrite(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	m_handler.stopConnection(shared_from_this());
    }
}

void
CommandConnection::onIncomingMessage(const EmsMessage& message)
{
    boost::tribool result = m_parser.onIncomingMessage(message);
    if (result == true) {
	respond("OK");
    } else if (result == false) {
	respond("FAIL");
    }
}

void
CommandConnection::onTimeout()
{
    if (m_parser.onTimeout()) {
	respond("ERRTIMEOUT");
    }
}
