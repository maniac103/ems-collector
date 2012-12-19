/*
 * Buderus EMS data collector
 *
 * Copyright (C) 2011 Danny Baumann <dannybaumann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <iomanip>
#include "IoHandler.h"
#include "Options.h"

IoHandler::IoHandler(Database& db) :
    boost::asio::io_service(),
    m_active(true),
    m_db(db),
    m_state(Syncing),
    m_pos(0)
{
    /* pre-alloc buffer to avoid reallocations */
    m_data.reserve(256);
}

IoHandler::~IoHandler()
{
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
		    m_state = Length;
		    m_pos = 0;
		} else {
		    m_pos = 0;
		}
		break;
	    case Length:
		m_state = Data;
		m_pos = 0;
		m_length = dataByte;
		m_checkSum = 0;
		break;
	    case Data:
		m_data.push_back(dataByte);
		m_checkSum ^= dataByte;
		m_pos++;
		if (m_pos == m_length) {
		    m_state = Checksum;
		}
		break;
	    case Checksum:
		if (m_checkSum == dataByte) {
		    EmsMessage message(&m_db, m_data);
		    message.handle();
		    if (message.getDestination() == EmsMessage::addressPC && m_pcMessageCallback) {
			m_pcMessageCallback(message);
		    }
		}
		m_data.clear();
		m_state = Syncing;
		m_pos = 0;
		break;
	}
    }

    readStart();
}

void
IoHandler::doClose(const boost::system::error_code& error)
{
    if (error && error != boost::asio::error::operation_aborted) {
	std::cerr << "Error: " << error.message() << std::endl;
    }

    doCloseImpl();
    m_active = false;
    stop();
}
