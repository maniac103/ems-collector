#include <cerrno>
#include <csignal>
#include <iostream>
#include <boost/thread.hpp>
#include "SerialHandler.h"
#include "Database.h"
#include "Options.h"
#include "PidFile.h"

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
	    SerialHandler handler(Options::deviceName(), db);

	    /* block all signals for background thread */
	    sigfillset(&newMask);
	    pthread_sigmask(SIG_BLOCK, &newMask, &oldMask);

	    /* run the IO service in background thread */
	    boost::thread t(boost::bind(&SerialHandler::run, &handler));

	    /* restore previous signals */
	    pthread_sigmask(SIG_SETMASK, &oldMask, 0);

	    /* wait for signal indicating time to shut down */
	    sigemptyset(&waitMask);
	    sigaddset(&waitMask, SIGINT);
	    sigaddset(&waitMask, SIGQUIT);
	    sigaddset(&waitMask, SIGTERM);

	    pthread_sigmask(SIG_BLOCK, &waitMask, 0);

	    while (handler.active()) {
		if (sigtimedwait(&waitMask, &info, &pollTimeout) >= 0) {
		    running = false;
		    handler.close();
		    break;
		}
	    }

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
