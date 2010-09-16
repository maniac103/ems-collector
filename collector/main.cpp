#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "SerialHandler.h"
#include "Database.h"
#include "Options.h"

int main(int argc, char *argv[])
{
    Options::ParseResult result = Options::parse(argc, argv);
    if (result == Options::ParseFailure) {
	return 1;
    } else if (result == Options::CloseAfterParse) {
	return 0;
    }

    try {
	boost::asio::io_service ioService;
	const std::string& dbPath = Options::databasePath();
	Database db;

	if (dbPath != "none") {
	    db = Database(dbPath, Options::databaseUser(), Options::databasePassword());
	    if (!db.isConnected()) {
		std::cerr << "Could not connect to database" << std::endl;
		return 1;
	    }
	}

	SerialHandler handler(ioService, Options::deviceName(), db);

	/* run the IO service as a separate thread,
	 * so the main thread can block on standard input */
	boost::thread t(boost::bind(&boost::asio::io_service::run, &ioService));

	while (handler.active()) {
	    char ch;

	    std::cin.get(ch);
	    if (ch == 3) {
		/* Ctrl-C */
		break;
	    }
	}

	handler.close();
	t.join();
    } catch (std::exception& e) {
	std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
