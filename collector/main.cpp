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
#include <unistd.h>
#include <boost/asio/signal_set.hpp>
#include <boost/scoped_ptr.hpp>
#include "SerialHandler.h"
#include "TcpHandler.h"
#include "Database.h"
#include "Options.h"
#include "PidFile.h"
#include "ValueCache.h"

static IoHandler *
getHandler(const std::string& target, const std::string& mqttTarget, Database& db, ValueCache& cache)
{
    IoHandler *handler = nullptr;

    if (target.compare(0, 7, "serial:") == 0) {
	handler = new SerialHandler(target.substr(7), db, cache);
    } else if (target.compare(0, 4, "tcp:") == 0) {
	size_t pos = target.find(':', 4);
	if (pos != std::string::npos) {
	    std::string host = target.substr(4, pos - 4);
	    std::string port = target.substr(pos + 1);
	    handler = new TcpHandler(host, port, db, cache);
	}
    }

    size_t pos = mqttTarget.find(':');
    if (handler && pos != std::string::npos) {
	std::string host = mqttTarget.substr(0, pos);
	std::string port = mqttTarget.substr(pos + 1);
	handler->setMqttAdapter(new MqttAdapter(*handler, host, port));
    }

    return handler;
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

	while (running) {
	    boost::scoped_ptr<IoHandler> handler(
		    getHandler(Options::target(), Options::mqttTarget(), db, cache));
	    if (!handler) {
		std::ostringstream msg;
		msg << "Target " << Options::target() << " is invalid.";
		throw std::runtime_error(msg.str());
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
