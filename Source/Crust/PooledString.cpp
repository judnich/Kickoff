#include "PooledString.h"
#include "Util.h"


class StringTable
{
public:
    StringTable() : m_autoCleanupCounter(0) {}

    static StringTable& singleton();
    std::pair<uint64_t, std::shared_ptr<std::string>> get(const std::string& str);
    void cleanup();

private:
    std::map<uint64_t, std::shared_ptr<std::string>> m_trackedStrings;
    int m_autoCleanupCounter;
};

static StringTable gSingleton;
static const int AUTO_CLEANUP_COUNT_THRESHOLD = 100;


StringTable& StringTable::singleton()
{
    return gSingleton;
}

std::pair<uint64_t, std::shared_ptr<std::string>> StringTable::get(const std::string& str)
{
    std::hash<std::string> strHash;
    uint64_t hash = strHash(str);
    auto it = m_trackedStrings.find(hash);
    if (it == m_trackedStrings.end()) {
        m_autoCleanupCounter++;
        if (m_autoCleanupCounter > AUTO_CLEANUP_COUNT_THRESHOLD) {
            cleanup();
        }

        auto strPtr = std::make_shared<std::string>(str);
        m_trackedStrings[hash] = strPtr;
        return std::make_pair(hash, strPtr);
    }
    else {
        return std::make_pair(hash, it->second);
    }
}

void StringTable::cleanup()
{
    std::vector<uint64_t> deadStrings;

    for (auto& entry : m_trackedStrings) {
        auto& ptr = entry.second;
        if (ptr.unique()) {
            deadStrings.push_back(entry.first);
        }
    }

    for (uint64_t hash : deadStrings) {
        m_trackedStrings.erase(hash);
    }

    m_autoCleanupCounter = 0;
}


PooledString::PooledString(const std::string& val)
{
    auto r = StringTable::singleton().get(val);
    m_hash = r.first;
    m_str = r.second;
}

PooledString::PooledString(const char* cstr)
{
    auto r = StringTable::singleton().get(std::string(cstr));
    m_hash = r.first;
    m_str = r.second;
}
