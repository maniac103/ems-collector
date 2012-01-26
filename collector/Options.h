#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include <iostream>
#include <fstream>

class DebugStream : public std::ostream
{
    public:
	DebugStream() :
	    std::ostream(std::cout.rdbuf()),
	    m_active(false)
	{ }

	void reset() {
	    m_active = false;
	    rdbuf(std::cout.rdbuf());
	}

	void setFile(const std::string& file) {
	    if (file == "stdout") {
		rdbuf(std::cout.rdbuf());
	    } else if (file == "stderr") {
		rdbuf(std::cerr.rdbuf());
	    } else if (!file.empty()) {
		m_fileBuf.open(file.c_str(), std::ios::out | std::ios::app);
		rdbuf(&m_fileBuf);
	    }
	    m_active = true;
	}

	operator bool() const {
	    return m_active;
	}

    private:
	bool m_active;
	std::filebuf m_fileBuf;
};

class Options
{
    public:
	typedef enum {
	    ParseFailure,
	    ParseSuccess,
	    CloseAfterParse
	} ParseResult;

	static unsigned int rateLimit() {
	    return m_rateLimit;
	}

	static const std::string& target() {
	    return m_target;
	}
	static bool daemonize() {
	    return m_daemonize;
	}
	static const std::string& pidFilePath() {
	    return m_pidFilePath;
	}
	static const std::string& databasePath() {
	    return m_dbPath;
	}
	static const std::string& databaseUser() {
	    return m_dbUser;
	}
	static const std::string& databasePassword() {
	    return m_dbPass;
	}

	static ParseResult parse(int argc, char *argv[]);

    private:
	static const unsigned int DebugIo = 0;
	static const unsigned int DebugMessages = 1;
	static const unsigned int DebugData = 2;
	static const unsigned int DebugStats = 3;
	static const unsigned int DebugCount = 4;
	static DebugStream m_debugStreams[DebugCount];

    public:
	static DebugStream& ioDebug() {
	    return m_debugStreams[DebugIo];
	}
	static DebugStream& messageDebug() {
	    return m_debugStreams[DebugMessages];
	}
	static DebugStream& dataDebug() {
	    return m_debugStreams[DebugData];
	}
	static DebugStream& statsDebug() {
	    return m_debugStreams[DebugStats];
	}

    private:
	static std::string m_target;
	static unsigned int m_rateLimit;
	static std::string m_pidFilePath;
	static bool m_daemonize;
	static std::string m_dbPath;
	static std::string m_dbUser;
	static std::string m_dbPass;
};

#endif /* __OPTIONS_H__ */
