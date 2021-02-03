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

#include <boost/bind/bind.hpp>
#include "CommandScheduler.h"

void
EmsCommandSender::handlePcMessage(const EmsMessage& message)
{
    m_lastCommTimes[message.getSource()] = boost::posix_time::microsec_clock::universal_time();
    m_responseTimeout.cancel();
    if (m_currentClient) {
	m_currentClient->onIncomingMessage(message);
    }
    continueWithNextRequest();
}

void
EmsCommandSender::sendMessage(ClientPtr& client, MessagePtr& message)
{
    bool wasIdle = !m_currentClient;
    m_pending.push_back(std::make_pair(client, message));
    if (wasIdle) {
	continueWithNextRequest();
    }
}

void
EmsCommandSender::scheduleResponseTimeout()
{
    m_responseTimeout.expires_from_now(boost::posix_time::milliseconds(RequestTimeout));
    m_responseTimeout.async_wait([this] (const boost::system::error_code& error) {
	if (error != boost::asio::error::operation_aborted) {
	    if (m_currentClient) {
		m_currentClient->onTimeout();
	    }
	    continueWithNextRequest();
	}
    });
}

void
EmsCommandSender::sendMessage(const MessagePtr& message)
{
    auto timeIter = m_lastCommTimes.find(message->getDestination());
    bool scheduled = false;

    if (timeIter != m_lastCommTimes.end()) {
	boost::posix_time::ptime now(boost::posix_time::microsec_clock::universal_time());
	boost::posix_time::time_duration diff = now - timeIter->second;

	if (diff.total_milliseconds() <= MinDistanceBetweenRequests) {
	    m_sendTimer.expires_at(timeIter->second + boost::posix_time::milliseconds(MinDistanceBetweenRequests));
	    m_sendTimer.async_wait(boost::bind(&EmsCommandSender::doSendMessage, this, *message));
	    scheduled = true;
	}
    }
    if (!scheduled) {
	doSendMessage(*message);
    }
}

void
EmsCommandSender::doSendMessage(const EmsMessage& message)
{
    sendMessageImpl(message);
    scheduleResponseTimeout();
    m_lastCommTimes[message.getDestination()] = boost::posix_time::microsec_clock::universal_time();
}

void
EmsCommandSender::continueWithNextRequest()
{
    if (m_pending.empty()) {
	m_currentClient.reset();
	return;
    }
    auto& item = m_pending.front();
    m_currentClient = item.first;
    sendMessage(item.second);
    m_pending.pop_front();
}
