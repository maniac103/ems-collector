#include <boost/algorithm/string.hpp>
#include "CommandHandler.h"

CommandHandler::CommandHandler(boost::asio::io_service& ioService,
			       boost::asio::ip::tcp::endpoint& endpoint) :
    m_service(ioService),
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
    CommandConnection::Ptr connection(new CommandConnection(*this, m_service));
    m_acceptor.async_accept(connection->socket(),
		            boost::bind(&CommandHandler::handleAccept, this,
					connection, boost::asio::placeholders::error));
}


CommandConnection::CommandConnection(CommandHandler& handler,
				     boost::asio::io_service& ioService) :
    m_socket(ioService),
    m_handler(handler)
{
}

void
CommandConnection::handleRead(const boost::system::error_code& error,
			      size_t bytesTransferred)
{
    if (error && error != boost::asio::error::operation_aborted) {
	m_handler.stopConnection(shared_from_this());
	return;
    }

    for (size_t i = 0; i < bytesTransferred; i++) {
	char item = m_buffer[i];

	if (!std::isalnum(item) && !std::isspace(item)) {
	    respond("INVALID");
	    break;
	}
	if (item == '\n') {
	    boost::trim(m_pendingCommand);
	    if (handleCommand(m_pendingCommand)) {
		respond("OK");
	    } else {
		respond("FAILURE");
	    }
	    m_pendingCommand.clear();
	    continue;
	}

	m_pendingCommand += item;
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

bool
CommandConnection::handleCommand(const std::string& command)
{
    return command != "SHOULDFAIL";
}
