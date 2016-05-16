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

#ifndef __SENDINGSERIALHANDLER_H__
#define __SENDINGSERIALHANDLER_H__

#include "CommandScheduler.h"
#include "SerialHandler.h"

class SendingSerialHandler : public SerialHandler, public EmsCommandSender
{
    public:
	SendingSerialHandler(const std::string& device, ValueCache& cache);

    protected:
	virtual void sendMessageImpl(const EmsMessage& msg) override;
	virtual void onPcMessageReceived(const EmsMessage& msg) override {
	    handlePcMessage(msg);
	}
};

#endif /* __SENDINGSERIALHANDLER_H__ */
