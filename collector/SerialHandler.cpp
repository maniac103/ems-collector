#include <iostream>
#include <iomanip>
#include "SerialHandler.h"
#include "Options.h"

SerialHandler::SerialHandler(const std::string& device,
			     Database& db) :
    boost::asio::io_service(),
    m_active(true),
    m_serialPort(*this, device),
    m_db(db),
    m_state(Syncing),
    m_pos(0),
    m_message(NULL)
{
    if (!m_serialPort.is_open()) {
	std::cerr << "Failed to open serial port." << std::endl;
	m_active = false;
	return;
    }

    boost::asio::serial_port_base::baud_rate baudOption(9600);
    m_serialPort.set_option(baudOption);

    readStart();
}

SerialHandler::~SerialHandler()
{
    if (m_message) {
	delete m_message;
    }
    if (m_active) {
	m_serialPort.close();
    }
}

void
SerialHandler::readComplete(const boost::system::error_code& error,
			    size_t bytesTransferred)
{
    size_t pos = 0;
    DebugStream& debug = Options::serialDebug();

    if (error) {
	doClose(error);
	return;
    }

    if (debug) {
	debug << "SERIAL: Got bytes ";
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
SerialHandler::doClose(const boost::system::error_code& error)
{
    if (error == boost::asio::error::operation_aborted) {
	/* if this call is the result of a timer cancel() */
	return; // ignore it because the connection cancelled the timer
    }

    if (error) {
	std::cerr << "Error: " << error.message() << std::endl;
    }

    m_serialPort.close();
    m_active = false;
}
