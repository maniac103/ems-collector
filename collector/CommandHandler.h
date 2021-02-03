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

#ifndef __COMMANDHANDLER_H__
#define __COMMANDHANDLER_H__

#include <set>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "ApiCommandParser.h"
#include "CommandScheduler.h"
#include "EmsMessage.h"
#include "Noncopyable.h"

class CommandHandler;

class CommandConnection : public boost::enable_shared_from_this<CommandConnection>,
			  private boost::noncopyable
{
    public:
	typedef boost::shared_ptr<CommandConnection> Ptr;

    public:
	CommandConnection(boost::asio::io_service& ios,
			  EmsCommandSender& sender,
			  CommandHandler& handler,
			  ValueCache *cache);

    public:
	boost::asio::ip::tcp::socket& socket() {
	    return m_socket;
	}
	void startRead() {
	    boost::asio::async_read_until(m_socket, m_request, "\n",
		boost::bind(&CommandConnection::handleRequest, shared_from_this(),
			    boost::asio::placeholders::error));
	}
	void close() {
	    m_socket.close();
	}
	void onIncomingMessage(const EmsMessage& message);
	void onTimeout();

    private:
	void handleRequest(const boost::system::error_code& error);
	void handleWrite(const boost::system::error_code& error);

	class CommandClient : public EmsCommandClient {
	    public:
		CommandClient(CommandConnection *connection) :
		    m_connection(connection)
		{}
		void onIncomingMessage(const EmsMessage& message) override {
		    m_connection->onIncomingMessage(message);
		}
		void onTimeout() {
		    m_connection->onTimeout();
		}

	    private:
		CommandConnection *m_connection;
	};

	void respond(const std::string& response) {
	    boost::asio::async_write(m_socket, boost::asio::buffer(response + "\n"),
		boost::bind(&CommandConnection::handleWrite, shared_from_this(),
			    boost::asio::placeholders::error));
	}

    private:
	boost::asio::ip::tcp::socket m_socket;
	boost::asio::streambuf m_request;
	boost::shared_ptr<EmsCommandClient> m_commandClient;
	ApiCommandParser m_parser;
	CommandHandler& m_handler;
};

class CommandHandler : private boost::noncopyable
{
    public:
	CommandHandler(boost::asio::io_service& ios,
		       EmsCommandSender& sender,
		       ValueCache *cache,
		       boost::asio::ip::tcp::endpoint& endpoint);
	~CommandHandler();

    public:
	void startConnection(CommandConnection::Ptr connection);
	void stopConnection(CommandConnection::Ptr connection);

    private:
	void handleAccept(CommandConnection::Ptr connection,
			  const boost::system::error_code& error);
	void startAccepting();

    private:
	boost::asio::io_service& m_ios;
	EmsCommandSender& m_sender;
	ValueCache *m_cache;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::set<CommandConnection::Ptr> m_connections;
};

#endif /* __COMMANDHANDLER_H__ */
