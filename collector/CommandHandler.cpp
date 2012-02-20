#include <boost/algorithm/string.hpp>
#include "CommandHandler.h"

CommandHandler::CommandHandler(boost::asio::io_service& ioService,
			       boost::asio::ip::tcp::socket& cmdSocket,
			       boost::asio::ip::tcp::endpoint& endpoint) :
    m_service(ioService),
    m_cmdSocket(cmdSocket),
    m_acceptor(ioService, endpoint)
{
    startAccepting();
}

CommandHandler::~CommandHandler()
{
    m_acceptor.close();
    std::for_each(m_connections.begin(), m_connections.end(),
		  boost::bind(&CommandConnection::close, _1));
    m_connections.clear();
}

void
CommandHandler::handleAccept(CommandConnection::Ptr connection,
			     const boost::system::error_code& error)
{
    if (error) {
	std::cerr << "Error: " << error.message() << std::endl;
    } else {
	startConnection(connection);
	startAccepting();
    }
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
    CommandConnection::Ptr connection(new CommandConnection(*this, m_service, m_cmdSocket));
    m_acceptor.async_accept(connection->socket(),
		            boost::bind(&CommandHandler::handleAccept, this,
					connection, boost::asio::placeholders::error));
}


CommandConnection::CommandConnection(CommandHandler& handler,
				     boost::asio::io_service& ioService,
				     boost::asio::ip::tcp::socket& cmdSocket) :
    m_socket(ioService),
    m_cmdSocket(cmdSocket),
    m_handler(handler)
{
}

void
CommandConnection::handleRequest(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	m_handler.stopConnection(shared_from_this());
	return;
    }

    std::istream requestStream(&m_request);
    boost::tribool result = handleCommand(requestStream);

    if (result) {
	respond("OK");
    } else if (!result) {
	respond("ERRFAIL");
    } else {
	respond("ERRCMD");
    }

    startRead();
}

void
CommandConnection::handleWrite(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	m_handler.stopConnection(shared_from_this());
    }
}

boost::tribool
CommandConnection::handleCommand(std::istream& request)
{
    std::string category;
    request >> category;

    if (category == "ww") {
	return handleWwCommand(request);
    }

    return boost::indeterminate;
}

boost::tribool
CommandConnection::handleWwCommand(std::istream& request)
{
    std::string cmd;
    request >> cmd;

    if (cmd == "mode") {
	std::vector<char> data = { 0x10, 0x37, 0x02 };
	std::string mode;
	
	request >> mode;

	if (mode == "on") {
	    data.push_back(0x01);
	} else if (mode == "off") {
	    data.push_back(0x00);
	} else if (mode == "auto") {
	    data.push_back(0x02);
	} else {
	    return boost::indeterminate;
	}
	return sendCommand(data);
    }

    return boost::indeterminate;
}

bool
CommandConnection::sendCommand(const std::vector<char>& data)
{
    boost::system::error_code error;
    boost::asio::write(m_cmdSocket, boost::asio::buffer(data), error);

    if (error) {
	std::cerr << "Command send error: " << error.message() << std::endl;
	return false;
    }

    return true;
}
