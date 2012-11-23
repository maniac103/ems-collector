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
#include "SerialHandler.h"
#include "Options.h"

SerialHandler::SerialHandler(const std::string& device,
			     Database& db) :
    IoHandler(db),
    m_serialPort(*this, device)
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
    if (m_active) {
	m_serialPort.close();
    }
}

void
SerialHandler::doCloseImpl()
{
    m_serialPort.close();
}
