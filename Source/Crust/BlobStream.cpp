#include "BlobStream.h"
#include "Error.h"


void BlobStreamWriter::reserve(size_t bytes)
{
    m_data.reserve(bytes);
}

BlobStreamWriter& BlobStreamWriter::operator<<(ArrayView<uint8_t> blob)
{
    if (blob.size() > 0) {
        size_t startOffset = m_data.size();
        size_t endOffset = startOffset + (size_t)blob.size();

        m_data.resize(endOffset);
        if (blob.size() > 0) {
            memcpy(&m_data[startOffset], &blob.first(), blob.size());
        }
    }

    return *this;
}

BlobStreamWriter& BlobStreamWriter::operator<<(const PooledBlob& blob)
{
    *this << (int)blob.get().size();
    return (*this << blob.get());
}

BlobStreamWriter& BlobStreamWriter::operator<<(const PooledString& blob)
{
    *this << (int)blob.get().size();
    return (*this << blob.getBytes());
}

BlobStreamWriter& BlobStreamWriter::operator<<(const std::string& blob)
{
    *this << (int)blob.size();
    ArrayView<uint8_t> bytes((const uint8_t*)blob.data(), int(blob.size() * sizeof(blob[1])));
    return (*this << bytes);
}


bool BlobStreamReader::operator>>(MutableArrayView<uint8_t> outBlob)
{
    if (m_data.size() < outBlob.size()) {
        return false;
    }
    memcpy(&outBlob.first(), &m_data.first(), outBlob.size());
    m_data = m_data.subView(outBlob.size(), m_data.size() - outBlob.size());
    return true;
}

bool BlobStreamReader::operator>>(std::string& outBlob)
{
    std::vector<char> strVec;
    if (!(*this >> strVec)) { return false; }
    outBlob = std::string(strVec.data(), strVec.size());
    return true;
}

bool BlobStreamReader::operator>> (PooledString& outBlob)
{
    std::string blobStr;
    if (!(*this >> blobStr)) { return false; }
    outBlob = PooledString(std::move(blobStr));
    return true;
}

bool BlobStreamReader::operator>> (PooledBlob& outBlob)
{
    std::vector<uint8_t> blobVec;
    if (!(*this >> blobVec)) { return false; }
    outBlob = PooledBlob(std::move(blobVec));
    return true;
}
