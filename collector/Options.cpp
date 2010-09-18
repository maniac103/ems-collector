#include <iostream>
#include <fstream>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include "Options.h"

std::string Options::m_deviceName;
unsigned int Options::m_rateLimit = 0;
DebugStream Options::m_debugStreams[DebugCount];
std::string Options::m_pidFilePath;
bool Options::m_daemonize = true;
std::string Options::m_dbPath;
std::string Options::m_dbUser;
std::string Options::m_dbPass;

static void
usage(std::ostream& stream, const char *programName,
      boost::program_options::options_description& options)
{
    stream << "Usage: " << programName << " [options] <serial_device>" << std::endl;
    stream << options << std::endl;
}

Options::ParseResult
Options::parse(int argc, char *argv[])
{
    std::string defaultPidFilePath;
    
    defaultPidFilePath = "/var/run/";
    defaultPidFilePath += argv[0];
    defaultPidFilePath += ".pid";

    boost::program_options::options_description general("General options");
    general.add_options()
	("help,h", "Show this help message")
	("ratelimit,r", boost::program_options::value<unsigned int>(&m_rateLimit)->default_value(60),
	 "Rate limit (in s) for writing sensors into DB")
	("debug,d", boost::program_options::value<std::string>()->default_value("none"),
	 "Comma separated list of debug flags (all, serial, message, data, none) "
	 " and their files, e.g. message=/tmp/messages.txt");

    boost::program_options::options_description daemon("Daemon options");
    daemon.add_options()
	("pid-file,p",
	 boost::program_options::value<std::string>(&m_pidFilePath)->default_value(defaultPidFilePath),
	 "Pid file path")
	("foreground,f", "Run in foreground");

    boost::program_options::options_description db("Database options");
    db.add_options()
	("db-path", boost::program_options::value<std::string>(&m_dbPath),
	 "Path or server:port specification of database server (none to not connect to DB)")
	("db-user,u", boost::program_options::value<std::string>(&m_dbUser),
	 "Database user name")
	("db-pass,P", boost::program_options::value<std::string>(&m_dbPass),
	 "Database password");

    boost::program_options::options_description hidden("Hidden options");
    hidden.add_options()
	("device", boost::program_options::value<std::string>(&m_deviceName), "Serial device to use");

    boost::program_options::options_description options;
    options.add(general);
    options.add(daemon);
    options.add(db);
    options.add(hidden);

    boost::program_options::options_description visible;
    visible.add(general);
    visible.add(daemon);
    visible.add(db);

    boost::program_options::positional_options_description p;
    p.add("device", 1);

    boost::program_options::variables_map variables;
    try {
	boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
				      options(options).positional(p).run(), variables);
	boost::program_options::notify(variables);
    } catch (boost::program_options::unknown_option& e) {
	usage(std::cerr, argv[0], visible);
	return ParseFailure;
    }

    if (variables.count("help")) {
	usage(std::cout, argv[0], visible);
	return CloseAfterParse;
    }

    /* check for missing variables */
    if (!variables.count("device")) {
	usage(std::cerr, argv[0], visible);
	return ParseFailure;
    }

    if (variables.count("foreground")) {
	m_daemonize = false;
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
	    boost::char_separator<char> sep(",");
	    boost::tokenizer<boost::char_separator<char> > tokens(flags, sep);
	    BOOST_FOREACH(const std::string& item, tokens) {
		std::string file;
		size_t start = item.find('=');
		unsigned int module;

		if (item.compare(0, 6, "serial") == 0) {
		    module = DebugSerial;
		} else if (item.compare(0, 7, "message") == 0) {
		    module = DebugMessages;
		} else if (item.compare(0, 4, "data") == 0) {
		    module = DebugData;
		} else {
		    continue;
		}

		if (start != std::string::npos) {
		    file = item.substr(start + 1);
		}
		m_debugStreams[module].setFile(file);
	    }
	}
    }

    return ParseSuccess;
}

