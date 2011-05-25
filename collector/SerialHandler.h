#ifndef __SERIALHANDLER_H__
#define __SERIALHANDLER_H__

#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include "Message.h"
#include "Database.h"

class SerialHandler : public boost::asio::io_service
{
    public:
	SerialHandler(const std::string& device, Database& db);
	~SerialHandler();

	void close() {
	    post(boost::bind(&SerialHandler::doClose, this,
			     boost::system::error_code()));
	}

	bool active() {
	    return m_active;
	}

    private:
	/* maximum amount of data to read in one operation */
	static const int maxReadLength = 512;

	void readStart() {
	    /* Start an asynchronous read and call read_complete when it completes or fails */
	    m_serialPort.async_read_some(boost::asio::buffer(m_recvBuffer, maxReadLength),
					 boost::bind(&SerialHandler::readComplete, this,
						     boost::asio::placeholders::error,
						     boost::asio::placeholders::bytes_transferred));
	}

	void readComplete(const boost::system::error_code& error, size_t bytesTransferred);
	void doClose(const boost::system::error_code& error);

    private:
	typedef enum {
	    Syncing,
	    Type,
	    Length,
	    Data,
	    Checksum
	} State;

	typedef enum {
	    DataPacket = 0,
	    StatsPacket = 1,
	    InvalidPacket = 2
	} PacketType;

	bool m_active;
	boost::asio::serial_port m_serialPort;

	Database& m_db;

	State m_state;
	PacketType m_type;
	size_t m_pos;
	Message *m_message;

	unsigned char m_recvBuffer[maxReadLength];
};

#endif /* __SERIALHANDLER_H__ */
