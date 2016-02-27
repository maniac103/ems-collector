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

#ifndef __APICOMMANDPARSER_H__
#define __APICOMMANDPARSER_H__

#include <boost/logic/tribool.hpp>
#include "CommandScheduler.h"
#include "ValueCache.h"

class ApiCommandParser : public boost::noncopyable
{
    public:
	typedef boost::function<void (const std::string& line)> OutputCallback;
	typedef enum {
	    Ok,
	    Busy,
	    InvalidCmd,
	    InvalidArgs
	} CommandResult;

    public:
	ApiCommandParser(EmsCommandSender& sender,
			 std::shared_ptr<EmsCommandClient>& client,
			 ValueCache& cache,
			 OutputCallback outputCb);

	CommandResult parse(std::istream& request);
	boost::tribool onIncomingMessage(const EmsMessage& message);
	bool onTimeout();

    public:
	static std::string buildRecordResponse(const EmsProto::ErrorRecord *record);
	static std::string buildRecordResponse(const EmsProto::ScheduleEntry *entry);
	static std::string buildRecordResponse(const char *type, const EmsProto::HolidayEntry *entry);

    private:
	CommandResult handleRcCommand(std::istream& request);
	CommandResult handleUbaCommand(std::istream& request);
#if defined(HAVE_RAW_READWRITE_COMMAND)
	CommandResult handleRawCommand(std::istream& request);
#endif
	CommandResult handleCacheCommand(std::istream& request);
	CommandResult handleHkCommand(std::istream& request, uint8_t base);
	CommandResult handleSingleByteValue(std::istream& request, uint8_t dest, uint8_t type,
					    uint8_t offset, int multiplier, int min, int max);
	CommandResult handleSetHolidayCommand(std::istream& request, uint8_t type, uint8_t offset);
	CommandResult handleWwCommand(std::istream& request);
	CommandResult handleThermDesinfectCommand(std::istream& request);
	CommandResult handleZirkPumpCommand(std::istream& request);

	template<typename T> boost::tribool loopOverResponse(const char *prefix = "");

	bool parseScheduleEntry(std::istream& request, EmsProto::ScheduleEntry *entry);
	bool parseHolidayEntry(const std::string& string, EmsProto::HolidayEntry *entry);

	boost::tribool handleResponse();
	void startRequest(uint8_t dest, uint8_t type, size_t offset, size_t length,
			  bool newRequest = true, bool raw = false);
	bool continueRequest();
	void sendCommand(uint8_t dest, uint8_t type, uint8_t offset,
			 const uint8_t *data, size_t count,
			 bool expectResponse = false);
	void sendActiveRequest();
	bool parseIntParameter(std::istream& request, uint8_t& data, uint8_t max);

	void output(const std::string& line) {
	    if (m_outputCb) {
		m_outputCb(line);
	    }
	}

    private:
	static const unsigned int MaxRequestRetries = 5;

	EmsCommandSender& m_sender;
	std::shared_ptr<EmsCommandClient> m_client;
	ValueCache& m_cache;
	OutputCallback m_outputCb;
	unsigned int m_responseCounter;
	unsigned int m_retriesLeft;
	std::shared_ptr<EmsMessage> m_activeRequest;
	std::vector<uint8_t> m_requestResponse;
	size_t m_requestOffset;
	size_t m_requestLength;
	uint8_t m_requestDestination;
	uint8_t m_requestType;
	size_t m_parsePosition;
	bool m_outputRawData;
};

#endif /* __APICOMMANDPARSER_H__ */
