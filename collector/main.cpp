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

#include <cerrno>
#include <csignal>
#include <iostream>
#include <boost/asio/signal_set.hpp>
#include <boost/scoped_ptr.hpp>
#include "CommandHandler.h"
#include "CommandScheduler.h"
#include "Database.h"
#include "DataHandler.h"
#include "MqttAdapter.h"
#include "Options.h"
#include "PidFile.h"
#include "SerialHandler.h"
#include "TcpHandler.h"
#include "ValueCache.h"

static IoHandler *
getHandler(const std::string& target, ValueCache& cache)
{
    if (target.compare(0, 7, "serial:") == 0) {
	return new SerialHandler(target.substr(7), cache);
    } else if (target.compare(0, 4, "tcp:") == 0) {
	size_t pos = target.find(':', 4);
	if (pos != std::string::npos) {
	    std::string host = target.substr(4, pos - 4);
	    std::string port = target.substr(pos + 1);
	    return new TcpHandler(host, port, cache);
	}
    }

    return nullptr;
}

static MqttAdapter *
getMqttAdapter(boost::asio::io_service& ios, EmsCommandSender *sender, const std::string& target)
{
    size_t pos = target.find(':');
    if (pos != std::string::npos) {
	std::string host = target.substr(0, pos);
	std::string port = target.substr(pos + 1);
	return new MqttAdapter(ios, sender, host, port);
    }

    return nullptr;
}

static void
stopHandler(IoHandler *handler, bool *running)
{
    *running = false;
    handler->stop();
}

int main(int argc, char *argv[])
{
    Options::ParseResult result = Options::parse(argc, argv);
    if (result == Options::ParseFailure) {
	return 1;
    } else if (result == Options::CloseAfterParse) {
	return 0;
    }

    try {
	const std::string& dbPath = Options::databasePath();
	Database db;
	ValueCache cache;
	bool running = true;

#ifdef HAVE_DAEMONIZE
	PidFile pid(Options::pidFilePath());
	if (Options::daemonize()) {
	    pid.aquire();
	}
#endif

	if (dbPath != "none") {
	    if (!db.connect(dbPath, Options::databaseUser(), Options::databasePassword())) {
		std::cerr << "Could not connect to database" << std::endl;
		return 1;
	    }
	}

#ifdef HAVE_DAEMONIZE
	if (Options::daemonize()) {
	    if (daemon(0, 0) == -1) {
		std::ostringstream msg;
		msg << "Could not daemonize: " << strerror(errno);
		throw std::runtime_error(msg.str());
	    }

	    pid.write();
	}
#endif

	IoHandler::ValueCallback dbValueCb = boost::bind(&Database::handleValue,&db, _1);
	IoHandler::ValueCallback cacheValueCb = boost::bind(&ValueCache::handleValue, &cache, _1);

	while (running) {
	    boost::scoped_ptr<IoHandler> handler(getHandler(Options::target(), cache));
	    if (!handler) {
		std::ostringstream msg;
		msg << "Target " << Options::target() << " is invalid.";
		throw std::runtime_error(msg.str());
	    }

	    handler->addValueCallback(dbValueCb);
	    handler->addValueCallback(cacheValueCb);

	    EmsCommandSender *sender = dynamic_cast<EmsCommandSender *>(handler.get());
	    boost::scoped_ptr<MqttAdapter> mqttAdapter(
		    getMqttAdapter(*handler, sender, Options::mqttTarget()));
	    if (mqttAdapter) {
		IoHandler::ValueCallback valueCb =
			boost::bind(&MqttAdapter::handleValue, mqttAdapter.get(), _1);
		handler->addValueCallback(valueCb);
	    }

	    boost::scoped_ptr<CommandHandler> cmdHandler;
	    unsigned int cmdPort = Options::commandPort();
	    if (sender && cmdPort != 0) {
		boost::asio::ip::tcp::endpoint cmdEndpoint(boost::asio::ip::tcp::v4(), cmdPort);
		cmdHandler.reset(new CommandHandler(*handler, *sender, &cache, cmdEndpoint));
	    }

	    boost::scoped_ptr<DataHandler> dataHandler;
	    unsigned int dataPort = Options::dataPort();
	    if (dataPort != 0) {
		boost::asio::ip::tcp::endpoint dataEndpoint(boost::asio::ip::tcp::v4(), dataPort);
		dataHandler.reset(new DataHandler(*handler, dataEndpoint));
		IoHandler::ValueCallback valueCb =
			boost::bind(&DataHandler::handleValue, dataHandler.get(), _1);
		handler->addValueCallback(valueCb);
	    }

	    boost::asio::signal_set signals(*handler, SIGINT, SIGTERM);
#ifdef SIGQUIT
	    signals.add(SIGQUIT);
#endif
	    signals.async_wait(boost::bind(&stopHandler, handler.get(), &running));

	    handler->run();

	    /* wait some time until retrying */
	    if (running) {
		sleep(10);
	    }
	}
    } catch (std::exception& e) {
	std::cerr << "Exception: " << e.what() << std::endl;
	return 1;
    }

    return 0;
}
