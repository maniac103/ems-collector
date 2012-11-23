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
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include "SerialHandler.h"
#include "TcpHandler.h"
#include "Database.h"
#include "Options.h"
#include "PidFile.h"

static IoHandler *
getHandler(const std::string& target, Database& db)
{
    if (target.compare(0, 7, "serial:") == 0) {
	return new SerialHandler(target.substr(7), db);
    }

    if (target.compare(0, 4, "tcp:") == 0) {
	size_t pos = target.find(':', 4);
	if (pos != std::string::npos) {
	    std::string host = target.substr(4, pos - 4);
	    std::string port = target.substr(pos + 1);
	    return new TcpHandler(host, port, db);
	}
    }

    return NULL;
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
	sigset_t oldMask, newMask, waitMask;
	struct timespec pollTimeout;
	siginfo_t info;
	const std::string& dbPath = Options::databasePath();
	PidFile pid(Options::pidFilePath());
	Database db;
	bool running = true;

	if (Options::daemonize()) {
	    pid.aquire();
	}

	if (dbPath != "none") {
	    if (!db.connect(dbPath, Options::databaseUser(), Options::databasePassword())) {
		std::cerr << "Could not connect to database" << std::endl;
		return 1;
	    }
	}

	if (Options::daemonize()) {
	    if (daemon(0, 0) == -1) {
		std::ostringstream msg;
		msg << "Could not daemonize: " << strerror(errno);
		throw std::runtime_error(msg.str());
	    }

	    pid.write();
	}

	pollTimeout.tv_sec = 2;
	pollTimeout.tv_nsec = 0;

	while (running) {
	    boost::scoped_ptr<IoHandler> handler(getHandler(Options::target(), db));
	    if (!handler) {
		std::ostringstream msg;
		msg << "Target " << Options::target() << " is invalid.";
		throw std::runtime_error(msg.str());
	    }

	    /* block all signals for background thread */
	    sigfillset(&newMask);
	    pthread_sigmask(SIG_BLOCK, &newMask, &oldMask);

	    /* run the IO service in background thread */
	    boost::thread t(boost::bind(&IoHandler::run, handler.get()));

	    /* restore previous signals */
	    pthread_sigmask(SIG_SETMASK, &oldMask, 0);

	    /* wait for signal indicating time to shut down */
	    sigemptyset(&waitMask);
	    sigaddset(&waitMask, SIGINT);
	    sigaddset(&waitMask, SIGQUIT);
	    sigaddset(&waitMask, SIGTERM);

	    pthread_sigmask(SIG_BLOCK, &waitMask, 0);

	    do {
		if (sigtimedwait(&waitMask, &info, &pollTimeout) >= 0) {
		    running = false;
		    handler->close();
		    break;
		}
	    } while (handler->active());

	    t.join();
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
