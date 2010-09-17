#include <iostream>
#include <iomanip>
#include "SerialHandler.h"
#include "Options.h"

SerialHandler::SerialHandler(boost::asio::io_service& ioService,
			     const std::string& device,
			     Database& db) :
    m_active(true),
    m_ioService(ioService),
    m_serialPort(ioService, device),
    m_db(db),
    m_state(Syncing),
    m_pos(0),
    m_message(NULL)
{
    if (!m_serialPort.is_open()) {
	std::cerr << "Failed to open serial port." << std::endl;
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
		    m_state = Length;
		    m_pos = 0;
		} else {
		    m_pos = 0;
		}
		break;
	    case Length:
		m_message = new Message(m_db, dataByte);
		m_state = Data;
		m_pos = 0;
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
    } else {
	m_serialPort.close();
	m_active = false;
    }
}
