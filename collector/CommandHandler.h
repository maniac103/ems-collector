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

class CommandHandler;

class CommandConnection : public boost::enable_shared_from_this<CommandConnection>,
			  private boost::noncopyable
{
    public:
	typedef boost::shared_ptr<CommandConnection> Ptr;

    public:
	CommandConnection(CommandHandler& handler,
			  boost::asio::io_service& ioService,
			  boost::asio::ip::tcp::socket& cmdSocket);

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
	    InvalidArgs,
	    Failed,
	    Waiting
	} CommandResult;

	CommandResult handleCommand(std::istream& request);
	CommandResult handleGetErrorsCommand();
	CommandResult handleHkCommand(std::istream& request, uint8_t base);
	CommandResult handleHkTemperatureCommand(std::istream& request, uint8_t base, uint8_t cmd);
	CommandResult handleWwCommand(std::istream& request);
	CommandResult handleThermDesinfectCommand(std::istream& request);
	CommandResult handleZirkPumpCommand(std::istream& request);

	void respond(const std::string& response) {
	    boost::asio::async_write(m_socket, boost::asio::buffer(response + "\n"),
		boost::bind(&CommandConnection::handleWrite, shared_from_this(),
			    boost::asio::placeholders::error));
	}
	CommandResult sendCommand(const std::vector<char>& data);

    private:
	boost::asio::ip::tcp::socket m_socket;
	boost::asio::ip::tcp::socket& m_cmdSocket;
	boost::asio::streambuf m_request;
	CommandHandler& m_handler;
};

class CommandHandler : private boost::noncopyable
{
    public:
	CommandHandler(boost::asio::io_service& ioService,
		       boost::asio::ip::tcp::socket& cmdSocket,
		       boost::asio::ip::tcp::endpoint& endpoint);
	~CommandHandler();

    public:
	void startConnection(CommandConnection::Ptr connection);
	void stopConnection(CommandConnection::Ptr connection);
	void handlePcMessage(const EmsMessage& message);

    private:
	void handleAccept(CommandConnection::Ptr connection,
			  const boost::system::error_code& error);
	void startAccepting();

    private:
	boost::asio::io_service& m_service;
	boost::asio::ip::tcp::socket& m_cmdSocket;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::set<CommandConnection::Ptr> m_connections;
};

#endif /* __COMMANDHANDLER_H__ */
