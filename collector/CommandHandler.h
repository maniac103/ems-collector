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
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include "EmsMessage.h"
#include "TcpHandler.h"

class CommandHandler;

class CommandConnection : public boost::enable_shared_from_this<CommandConnection>,
			  private boost::noncopyable
{
    public:
	typedef boost::shared_ptr<CommandConnection> Ptr;

    public:
	CommandConnection(CommandHandler& handler);
	~CommandConnection();

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
	void handlePcMessage(const EmsMessage& message);

    private:
	void handleRequest(const boost::system::error_code& error);
	void handleWrite(const boost::system::error_code& error);

	typedef enum {
	    Ok,
	    InvalidCmd,
	    InvalidArgs
	} CommandResult;

	CommandResult handleCommand(std::istream& request);
	CommandResult handleRcCommand(std::istream& request);
	CommandResult handleUbaCommand(std::istream& request);
	CommandResult handleHkCommand(std::istream& request, uint8_t base);
	CommandResult handleHkTemperatureCommand(std::istream& request, uint8_t base, uint8_t offset);
	CommandResult handleSetHolidayCommand(std::istream& request, uint8_t type, uint8_t offset);
	CommandResult handleWwCommand(std::istream& request);
	CommandResult handleThermDesinfectCommand(std::istream& request);
	CommandResult handleZirkPumpCommand(std::istream& request);

	template<typename T> bool loopOverResponse(const char *prefix = "");
	std::string buildRecordResponse(const EmsMessage::ErrorRecord *record);
	std::string buildRecordResponse(const EmsMessage::ScheduleEntry *entry);
	std::string buildRecordResponse(const char *type, const EmsMessage::HolidayEntry *entry);

	bool parseScheduleEntry(std::istream& request, EmsMessage::ScheduleEntry *entry);
	bool parseHolidayEntry(const std::string& string, EmsMessage::HolidayEntry *entry);

	void respond(const std::string& response) {
	    boost::asio::async_write(m_socket, boost::asio::buffer(response + "\n"),
		boost::bind(&CommandConnection::handleWrite, shared_from_this(),
			    boost::asio::placeholders::error));
	}
	void scheduleResponseTimeout();
	void responseTimeout(const boost::system::error_code& error);
	void startRequest(uint8_t dest, uint8_t type, size_t offset, size_t length, bool newRequest = true);
	bool continueRequest();
	void sendCommand(uint8_t dest, uint8_t type, uint8_t offset,
			 const uint8_t *data, size_t count,
			 bool expectResponse = false);
	bool parseIntParameter(std::istream& request, uint8_t& data, uint8_t max);

    private:
	boost::asio::ip::tcp::socket m_socket;
	boost::asio::streambuf m_request;
	CommandHandler& m_handler;
	bool m_waitingForResponse;
	boost::asio::deadline_timer m_responseTimeout;
	unsigned int m_responseCounter;
	std::vector<uint8_t> m_requestResponse;
	size_t m_requestOffset;
	size_t m_requestLength;
	uint8_t m_requestDestination;
	uint8_t m_requestType;
	size_t m_parsePosition;
};

class CommandHandler : private boost::noncopyable
{
    public:
	CommandHandler(TcpHandler& handler,
		       boost::asio::ip::tcp::endpoint& endpoint);
	~CommandHandler();

    public:
	void startConnection(CommandConnection::Ptr connection);
	void stopConnection(CommandConnection::Ptr connection);
	void handlePcMessage(const EmsMessage& message);
	TcpHandler& getHandler() const {
	    return m_handler;
	}
	void sendMessage(const EmsMessage& msg);

    private:
	void handleAccept(CommandConnection::Ptr connection,
			  const boost::system::error_code& error);
	void startAccepting();
	void doSendMessage(const EmsMessage& msg);

    private:
	static const long MinDistanceBetweenRequests = 100; /* ms */

    private:
	TcpHandler& m_handler;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::set<CommandConnection::Ptr> m_connections;
	boost::asio::deadline_timer m_sendTimer;
	std::map<uint8_t, boost::posix_time::ptime> m_lastCommTimes;
};

#endif /* __COMMANDHANDLER_H__ */
