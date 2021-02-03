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

#ifndef __VALUECACHE_H__
#define __VALUECACHE_H__

#include <time.h>
#include <map>
#include <ostream>
#include <vector>
#include "EmsMessage.h"

class ValueCache
{
    public:
	ValueCache();
	~ValueCache();

	void handleValue(const EmsValue& value);
	void outputValues(const std::vector<std::string>& selector, std::ostream& stream);
	const EmsValue * getValue(EmsValue::Type type, EmsValue::SubType subtype) const;

    private:
	class CacheKey {
	    public:
		CacheKey(EmsValue::Type type, EmsValue::SubType subtype) :
		    m_type(type), m_subtype(subtype) { }
		bool operator=(const CacheKey& rhs) {
		    return m_type == rhs.m_type && m_subtype == rhs.m_subtype;
		}
		bool operator<(const CacheKey& rhs) const {
		    if (m_type != rhs.m_type) {
			return m_type < rhs.m_type;
		    }
		    return m_subtype < rhs.m_subtype;
		}

		EmsValue::Type m_type;
		EmsValue::SubType m_subtype;
	};

	struct CacheEntry {
	    time_t timestamp;
	    EmsValue value;

	    CacheEntry(const EmsValue& v) :
		timestamp(time(NULL)), value(v) { }
	};

	std::map<CacheKey, CacheEntry> m_cache;
};

#endif /* __VALUECACHE_H__ */
