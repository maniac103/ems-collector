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

#include "ValueApi.h"
#include "ValueCache.h"

ValueCache::ValueCache()
{
}

ValueCache::~ValueCache()
{
}

void
ValueCache::handleValue(const EmsValue& value)
{
    CacheKey key(value.getType(), value.getSubType());
    m_cache.erase(key);
    m_cache.insert(std::make_pair(key, CacheEntry(value)));
}

const EmsValue *
ValueCache::getValue(EmsValue::Type type, EmsValue::SubType subtype) const
{
    auto iter = m_cache.find(CacheKey(type, subtype));
    if (iter == m_cache.end()) {
	return NULL;
    }
    return &iter->second.value;
}

void
ValueCache::outputValues(const std::vector<std::string>& selector, std::ostream& stream)
{
    for (auto& entry: m_cache) {
	std::string type = ValueApi::getTypeName(entry.first.m_type);
	if (type.empty()) {
	    continue;
	}

	std::string subtype = ValueApi::getSubTypeName(entry.first.m_subtype);
	bool matchesSelector = false;

	if (selector.size() >= 1) {
	    if (selector[0] == type) {
		matchesSelector = true;
	    } else if (selector[0] == subtype || (selector[0] == "none" && subtype.empty())) {
		if (selector.size() == 1) {
		    matchesSelector = true;
		} else if (selector[1] == type) {
		    matchesSelector = true;
		}
	    }
	} else {
	    // no selector matches everything
	    matchesSelector = true;
	}
	if (!matchesSelector) {
	    continue;
	}

	if (!subtype.empty()) {
	    stream << subtype << " ";
	}
	stream << type << " = " << ValueApi::formatValue(entry.second.value);
	stream << " | " << entry.second.timestamp << '\n';
    }
}
