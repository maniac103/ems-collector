#ifndef __SERIALHANDLER_H__
#define __SERIALHANDLER_H__

#include <boost/asio/serial_port.hpp>
#include "IoHandler.h"

class SerialHandler : public IoHandler
{
    public:
	SerialHandler(const std::string& device, Database& db);
	~SerialHandler();

    protected:
	virtual void readStart() {
	    /* Start an asynchronous read and call read_complete when it completes or fails */
	    m_serialPort.async_read_some(boost::asio::buffer(m_recvBuffer, maxReadLength),
					 boost::bind(&SerialHandler::readComplete, this,
						     boost::asio::placeholders::error,
						     boost::asio::placeholders::bytes_transferred));
	}

	virtual void doCloseImpl();

    private:
	boost::asio::serial_port m_serialPort;
};

#endif /* __SERIALHANDLER_H__ */
