#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include "Options.h"

std::string Options::m_deviceName;
unsigned int Options::m_rateLimit = 0;
DebugStream Options::m_debugStreams[DebugCount];
std::string Options::m_dbPath;
std::string Options::m_dbUser;
std::string Options::m_dbPass;

Options::ParseResult
Options::parse(int argc, char *argv[])
{
    boost::program_options::options_description general("General options");
    general.add_options()
	("help", "Show this help message")
	("ratelimit", boost::program_options::value<unsigned int>(&m_rateLimit)->default_value(0),
	 "Rate limit (in s) for writing sensors into DB");

    boost::program_options::options_description debug("Debug options");
    debug.add_options()
	("debug", boost::program_options::value<std::string>()->default_value("none"),
	 "Comma separated list of debug flags (all, serial, message, data, none) "
	 " and their files, e.g. message=/tmp/messages.txt");

    boost::program_options::options_description db("Database options");
    db.add_options()
	("db-path", boost::program_options::value<std::string>(&m_dbPath),
	 "Path or server:port specification of database server (none to not connect to DB)")
	("db-user", boost::program_options::value<std::string>(&m_dbUser),
	 "Database user name")
	("db-pass", boost::program_options::value<std::string>(&m_dbPass),
	 "Database password");

    boost::program_options::options_description hidden("Hidden options");
    hidden.add_options()
	("device", boost::program_options::value<std::string>(&m_deviceName), "Serial device to use");

    boost::program_options::options_description options;
    options.add(general);
    options.add(debug);
    options.add(db);
    options.add(hidden);

    boost::program_options::options_description visible;
    visible.add(general);
    visible.add(debug);
    visible.add(db);

    boost::program_options::positional_options_description p;
    p.add("device", 1);

    boost::program_options::variables_map variables;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
				  options(options).positional(p).run(), variables);
    boost::program_options::notify(variables);

    if (variables.count("help")) {
	std::cout << "Usage: " << argv[0] << " [options] <serial_device>" << std::endl;
	std::cout << visible << std::endl;
	return CloseAfterParse;
    }

    /* check for missing variables */
    if (!variables.count("device")) {
	std::cerr << "Usage: " << argv[0] << " [options] <serial_device>" << std::endl;
	std::cerr << visible << std::endl;
	return ParseFailure;
    }

    if (variables.count("debug")) {
	std::string flags = variables["debug"].as<std::string>();
	if (flags == "none") {
	    for (unsigned int i = 0; i < DebugCount; i++) {
		m_debugStreams[i].reset();
	    }
	} else if (flags.substr(0, 3) == "all") {
	    size_t start = flags.find('=', 3);
	    std::string file;
	    if (start != std::string::npos) {
		file = flags.substr(start + 1);
	    }
	    for (unsigned int i = 0; i < DebugCount; i++) {
		m_debugStreams[i].setFile(file);
	    }
	} else {
	    size_t lastPos = 0, pos;
	    while ((pos = flags.find(',', lastPos)) != std::string::npos) {
		parseDebugDefinition(flags.substr(lastPos, pos - lastPos));
		lastPos = pos + 1;
	    }
	    if (lastPos < flags.length()) {
		parseDebugDefinition(flags.substr(lastPos));
	    }
	}
    }

    return ParseSuccess;
}

void
Options::parseDebugDefinition(const std::string& definition)
{
    std::string file;
    size_t start = definition.find('=');
    unsigned int module;

    if (definition.compare(0, 6, "serial") == 0) {
	module = DebugSerial;
    } else if (definition.compare(0, 7, "message") == 0) {
	module = DebugMessages;
    } else if (definition.compare(0, 4, "data") == 0) {
	module = DebugData;
    } else {
	return;
    }

    if (start != std::string::npos) {
	file = definition.substr(start + 1);
    }
    m_debugStreams[module].setFile(file);
}

