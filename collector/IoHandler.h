#ifndef __IOHANDLER_H__
#define __IOHANDLER_H__

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <fstream>
#include "Database.h"
#include "EmsMessage.h"

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
	boost::function<void (const EmsMessage& message)> m_pcMessageCallback;

    private:
	typedef enum {
	    Syncing,
	    Length,
	    Data,
	    Checksum
	} State;

	Database& m_db;

	State m_state;
	size_t m_pos, m_length;
	uint8_t m_checkSum;
	std::vector<uint8_t> m_data;
};

#endif /* __IOHANDLER_H__ */
