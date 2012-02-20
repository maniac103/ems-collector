#include <iostream>
#include <iomanip>
#include "TcpHandler.h"

TcpHandler::TcpHandler(const std::string& host,
		       const std::string& port,
		       Database& db) :
    IoHandler(db),
    m_socket(*this)
{
    boost::asio::ip::tcp::resolver resolver(*this);
    boost::asio::ip::tcp::resolver::query query(host, port);
    boost::asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(query);

    boost::asio::async_connect(m_socket, endpoint,
			       boost::bind(&TcpHandler::handleConnect, this,
					   boost::asio::placeholders::error));
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
	m_cmdHandler.reset(new CommandHandler(*this, m_socket, cmdEndpoint));
	readStart();
    }
}

void
TcpHandler::doCloseImpl()
{
    m_cmdHandler.reset();
    m_socket.close();
}
