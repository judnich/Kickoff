#pragma once

#include <map>
#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include "Array.h"


typedef std::vector<uint8_t> ByteVector;
typedef std::shared_ptr<ByteVector> ByteVectorPtr;

// This is an immutable "pooled blob" class which if very memory-efficient to represent many
// copies of the same binary data. Copying a PooledBlob only creates a few extra bytes to track
// the internal shared data. Even constructing a new PooledBlob that matches existing ones will
// not allocate a duplicate copy of the string's bytes, since internal tracking of content hashes
// is used to eliminate all string duplication. Bonus feature: A PooledBlob also stores the data's
// hash, so getting a hash of a PooledBlob is very fast constant time.
class PooledBlob
{
public:
    PooledBlob();
    PooledBlob(ArrayView<uint8_t> data);
    PooledBlob(const std::vector<uint8_t>& data);

    ArrayView<uint8_t> get() const { return ArrayView<uint8_t>(*m_bytes); }
    uint64_t getHash() const { return m_hash; }

private:
    ByteVectorPtr m_bytes;
    uint64_t m_hash;
};

// Implement std::hash for PooledBlob so it can be used in hash maps, sets, etc
namespace std {
    template <> struct hash<PooledBlob>
    {
        size_t operator()(const PooledBlob& x) const { return x.getHash(); }
    };
}
