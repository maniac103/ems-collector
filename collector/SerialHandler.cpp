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
