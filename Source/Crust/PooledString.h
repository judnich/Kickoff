#pragma once

#include <map>
#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include "Array.h"


// This is an immutable "shared string" class which if very memory-efficient to represent many
// copies of the same strings. Copying a CompactString only creates a few extra bytes to track
// the internal shared string data. Even constructing a new CompactString that matches existing
// ones will not allocate a duplicate copy of the string's bytes, since an internal hash map is
// used to eliminate all string duplication. Bonus feature: A SharedString also stores the string's
// hash, so getting the hash of a SharedString is very fast constant time.
class PooledString
{
public:
    PooledString(const std::string& val = "");
    PooledString(const char* cstr);

    std::string get() { return *m_str; }
    const std::string& get() const { return *m_str; }

    ArrayView<uint8_t> getBytes() const
    {
        return ArrayView<uint8_t>(
            (const uint8_t*)m_str->data(),
            int(m_str->size() * sizeof((*m_str)[0]))
        );
    }

    uint64_t getHash() const { return m_hash; }
    bool operator< (const PooledString& other) const { return *m_str < *other.m_str; }
    bool operator<= (const PooledString& other) const { return *m_str <= *other.m_str; }
    bool operator>= (const PooledString& other) const { return *m_str >= *other.m_str; }
    bool operator>(const PooledString& other) const { return *m_str > *other.m_str; }
    bool operator== (const PooledString& other) const { return getHash() == other.getHash() && *m_str == *other.m_str; }
    bool operator!= (const PooledString& other) const { return getHash() != other.getHash() || *m_str != *other.m_str; }

private:
    std::shared_ptr<std::string> m_str;
    uint64_t m_hash;
};

// Implement std::hash for SharedString so it can be used in hash maps, sets, etc
namespace std {
    template <> struct hash<PooledString>
    {
        size_t operator()(const PooledString& x) const { return x.getHash(); }
    };
}
