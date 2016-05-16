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

#include <iomanip>
#include "Options.h"
#include "SendingSerialHandler.h"

SendingSerialHandler::SendingSerialHandler(const std::string& device,
					   ValueCache& cache) :
    SerialHandler(device, cache),
    EmsCommandSender((boost::asio::io_service&) *this)
{
}

void
SendingSerialHandler::sendMessageImpl(const EmsMessage& msg)
{
    boost::system::error_code error;
    std::vector<uint8_t> sendData = msg.getSendData(false);
    DebugStream& debug = Options::ioDebug();
    uint8_t checksum = 0;

    if (!m_active) {
	return;
    }

    for (size_t i = 0; i < sendData.size(); i++) {
	checksum ^= sendData[i];
    }
    sendData.insert(sendData.begin(), { 0xaa, 0x55, static_cast<uint8_t>(sendData.size()) });
    sendData.push_back(checksum);

    if (debug) {
	debug << "IO: Sending bytes ";
	for (size_t i = 0; i < sendData.size(); i++) {
	    debug << std::setfill('0') << std::setw(2)
		  << std::showbase << std::hex
		  << (unsigned int) sendData[i] << " ";
	}
	debug << std::endl;
    }

    boost::asio::write(m_serialPort, boost::asio::buffer(sendData),
		       boost::asio::transfer_all(), error);
}
