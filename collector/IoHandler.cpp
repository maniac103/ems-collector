#include <iostream>
#include <iomanip>
#include "IoHandler.h"
#include "Options.h"

IoHandler::IoHandler(Database& db) :
    boost::asio::io_service(),
    m_active(true),
    m_db(db),
    m_state(Syncing),
    m_pos(0),
    m_message(NULL)
{
}

IoHandler::~IoHandler()
{
    if (m_message) {
	delete m_message;
    }
}

void
IoHandler::readComplete(const boost::system::error_code& error,
			size_t bytesTransferred)
{
    size_t pos = 0;
    DebugStream& debug = Options::ioDebug();

    if (error) {
	doClose(error);
	return;
    }

    if (debug) {
	debug << "IO: Got bytes ";
	for (size_t i = 0; i < bytesTransferred; i++) {
	    debug << std::setfill('0') << std::setw(2)
		  << std::showbase << std::hex
		  << (unsigned int) m_recvBuffer[i] << " ";
	}
	debug << std::endl;
    }

    while (pos < bytesTransferred) {
	unsigned char dataByte = m_recvBuffer[pos++];

	switch (m_state) {
	    case Syncing:
		if (m_pos == 0 && dataByte == 0xaa) {
		    m_pos = 1;
		} else if (m_pos == 1 && dataByte == 0x55) {
		    m_state = Type;
		    m_pos = 0;
		} else {
		    m_pos = 0;
		}
		break;
	    case Type:
		if (dataByte >= InvalidPacket) {
		    m_state = Syncing;
		} else {
		    m_type = (PacketType) dataByte;
		    m_state = Length;
		}
		break;
	    case Length:
		m_state = Data;
		m_pos = 0;
		switch (m_type) {
		    case DataPacket:
			m_message = new DataMessage(m_db, dataByte);
			break;
		    case StatsPacket:
			m_message = new StatsMessage(m_db, dataByte);
			break;
		    default:
			assert(0);
			m_state = Syncing;
			break;
		}
		break;
	    case Data:
		m_message->addData(dataByte);
		if (m_message->isFull()) {
		    m_state = Checksum;
		}
		break;
	    case Checksum:
		if (m_message->checksumMatches(dataByte)) {
		    m_message->parse();
		}
		delete m_message;
		m_message = NULL;
		m_state = Syncing;
		break;
	}
    }

    readStart();
}

void
IoHandler::doClose(const boost::system::error_code& error)
{
    if (error == boost::asio::error::operation_aborted) {
	/* if this call is the result of a timer cancel() */
	return; // ignore it because the connection cancelled the timer
    }

    if (error) {
	std::cerr << "Error: " << error.message() << std::endl;
    }

    doCloseImpl();
    m_active = false;
}
