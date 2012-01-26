#ifndef __IOHANDLER_H__
#define __IOHANDLER_H__

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include "Message.h"
#include "Database.h"

class IoHandler : public boost::asio::io_service
{
    public:
	IoHandler(Database& db);
	~IoHandler();

	void close() {
	    post(boost::bind(&IoHandler::doClose, this,
			     boost::system::error_code()));
	}

	bool active() {
	    return m_active;
	}

    protected:
	/* maximum amount of data to read in one operation */
	static const int maxReadLength = 512;

	virtual void readStart() = 0;
	virtual void doCloseImpl() = 0;

	void readComplete(const boost::system::error_code& error, size_t bytesTransferred);
	void doClose(const boost::system::error_code& error);

	bool m_active;
	unsigned char m_recvBuffer[maxReadLength];

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

	Database& m_db;

	State m_state;
	PacketType m_type;
	size_t m_pos;
	Message *m_message;
};

#endif /* __IOHANDLER_H__ */
