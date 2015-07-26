#include "PooledBlob.h"
#include "Util.h"


class BlobTable
{
public:
    BlobTable() : m_autoCleanupCounter(0) {}

    static BlobTable& singleton();
    std::pair<uint64_t, ByteVectorPtr> get(ArrayView<uint8_t> data);
    void cleanup();

private:
    std::map<uint64_t, ByteVectorPtr> m_trackedBlobs;
    int m_autoCleanupCounter;
};

static BlobTable gSingleton;
static const int AUTO_CLEANUP_COUNT_THRESHOLD = 100;


BlobTable& BlobTable::singleton()
{
    return gSingleton;
}

std::pair<uint64_t, ByteVectorPtr> BlobTable::get(ArrayView<uint8_t> data)
{
    uint64_t hash = hashData(data);

    auto it = m_trackedBlobs.find(hash);
    if (it == m_trackedBlobs.end()) {
        m_autoCleanupCounter++;
        if (m_autoCleanupCounter > AUTO_CLEANUP_COUNT_THRESHOLD) {
            cleanup();
        }

        auto blobPtr = std::make_shared<ByteVector>(data.size());
        memcpy(blobPtr->data(), &data.first(), data.size());

        m_trackedBlobs[hash] = blobPtr;
        return std::make_pair(hash, blobPtr);
    }
    else {
        return std::make_pair(hash, it->second);
    }
}

void BlobTable::cleanup()
{
    std::vector<uint64_t> deadBlobs;

    for (auto& entry : m_trackedBlobs) {
        auto& ptr = entry.second;
        if (ptr.unique()) {
            deadBlobs.push_back(entry.first);
        }
    }

    for (uint64_t hash : deadBlobs) {
        m_trackedBlobs.erase(hash);
    }

    m_autoCleanupCounter = 0;
}


PooledBlob::PooledBlob(ArrayView<uint8_t> data)
{
    auto r = BlobTable::singleton().get(data);
    m_hash = r.first;
    m_bytes = r.second;
}

PooledBlob::PooledBlob(const std::vector<uint8_t>& data)
{
    auto r = BlobTable::singleton().get(data);
    m_hash = r.first;
    m_bytes = r.second;
}

PooledBlob::PooledBlob()
{
    m_hash = 0;
}
