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
