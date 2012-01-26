#include <iostream>
#include <fstream>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include "Options.h"

namespace bpo = boost::program_options;

std::string Options::m_target;
unsigned int Options::m_rateLimit = 0;
DebugStream Options::m_debugStreams[DebugCount];
std::string Options::m_pidFilePath;
bool Options::m_daemonize = true;
std::string Options::m_dbPath;
std::string Options::m_dbUser;
std::string Options::m_dbPass;

static void
usage(std::ostream& stream, const char *programName,
      bpo::options_description& options)
{
    stream << "Usage: " << programName << " [options] <target>" << std::endl;
    stream << options << std::endl;
}

Options::ParseResult
Options::parse(int argc, char *argv[])
{
    std::string defaultPidFilePath;
    std::string config;

    defaultPidFilePath = "/var/run/";
    defaultPidFilePath += argv[0];
    defaultPidFilePath += ".pid";

    bpo::options_description general("General options");
    general.add_options()
	("help,h", "Show this help message")
	("ratelimit,r", bpo::value<unsigned int>(&m_rateLimit)->default_value(60),
	 "Rate limit (in s) for writing numeric sensor values into DB")
	("debug,d", bpo::value<std::string>()->default_value("none"),
	 "Comma separated list of debug flags (all, io, message, data, stats, none) "
	 " and their files, e.g. message=/tmp/messages.txt");

    bpo::options_description daemon("Daemon options");
    daemon.add_options()
	("pid-file,P",
	 bpo::value<std::string>(&m_pidFilePath)->default_value(defaultPidFilePath),
	 "Pid file path")
	("foreground,f", "Run in foreground")
	("config-file,c", bpo::value<std::string>(&config),
	 "File name to read configuration from");

    bpo::options_description db("Database options");
    db.add_options()
	("db-path", bpo::value<std::string>(&m_dbPath)->composing(),
	 "Path or server:port specification of database server (none to not connect to DB)")
	("db-user,u", bpo::value<std::string>(&m_dbUser)->composing(),
	 "Database user name")
	("db-pass,p", bpo::value<std::string>(&m_dbPass)->composing(),
	 "Database password");

    bpo::options_description hidden("Hidden options");
    hidden.add_options()
	("target", bpo::value<std::string>(&m_target), "Connection target (serial:<device> or tcp:<host>:<port>)");

    bpo::options_description options;
    options.add(general);
    options.add(daemon);
    options.add(db);
    options.add(hidden);

    bpo::options_description configOptions;
    configOptions.add(general);
    configOptions.add(db);

    bpo::options_description visible;
    visible.add(general);
    visible.add(daemon);
    visible.add(db);

    bpo::positional_options_description p;
    p.add("target", 1);

    bpo::variables_map variables;
    try {
	bpo::store(bpo::command_line_parser(argc, argv).options(options).positional(p).run(),
		   variables);
	bpo::notify(variables);

	if (!config.empty()) {
	    std::ifstream configFile(config.c_str());
	    bpo::store(bpo::parse_config_file(configFile, configOptions), variables);
	    bpo::notify(variables);
	}
    } catch (bpo::unknown_option& e) {
	usage(std::cerr, argv[0], visible);
	return ParseFailure;
    } catch (bpo::multiple_occurrences& e) {
	usage(std::cerr, argv[0], visible);
	return ParseFailure;
    }

    if (variables.count("help")) {
	usage(std::cout, argv[0], visible);
	return CloseAfterParse;
    }

    /* check for missing variables */
    if (!variables.count("target")) {
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

		if (item.compare(0, 6, "io") == 0) {
		    module = DebugIo;
		} else if (item.compare(0, 7, "message") == 0) {
		    module = DebugMessages;
		} else if (item.compare(0, 4, "data") == 0) {
		    module = DebugData;
		} else if (item.compare(0, 5, "stats") == 0) {
		    module = DebugStats;
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

