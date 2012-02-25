#ifndef __TCPHANDLER_H__
#define __TCPHANDLER_H__

#include "IoHandler.h"
#include "CommandHandler.h"
#include <boost/shared_ptr.hpp>

class TcpHandler : public IoHandler
{
    public:
	TcpHandler(const std::string& host, const std::string& port, Database& db);
	~TcpHandler();

    protected:
	virtual void readStart() {
	    /* Start an asynchronous read and call read_complete when it completes or fails */
	    m_socket.async_read_some(boost::asio::buffer(m_recvBuffer, maxReadLength),
				     boost::bind(&TcpHandler::readComplete, this,
						 boost::asio::placeholders::error,
						 boost::asio::placeholders::bytes_transferred));
	}

	virtual void doCloseImpl();

    private:
	void handleConnect(const boost::system::error_code& error);

    private:
	boost::asio::ip::tcp::socket m_socket;
	boost::shared_ptr<CommandHandler> m_cmdHandler;
};

#endif /* __TCPHANDLER_H__ */
