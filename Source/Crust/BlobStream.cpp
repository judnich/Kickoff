#include "BlobStream.h"
#include "Error.h"


void BlobStreamWriter::reserve(size_t bytes)
{
    m_data.reserve(bytes);
}

BlobStreamWriter& BlobStreamWriter::operator<<(ArrayView<uint8_t> blob)
{
    size_t sizeStartOffset = m_data.size();
    size_t dataStartOffset = sizeStartOffset + sizeof(int);
    size_t endOffset = dataStartOffset + (size_t)blob.size();
    int blobSize = blob.size();

    m_data.resize(endOffset);
    memcpy(&m_data[sizeStartOffset], &blobSize, sizeof(blobSize));
    if (blobSize > 0) {
        memcpy(&m_data[dataStartOffset], &blob.first(), blobSize);
    }

    return *this;
}

BlobStreamWriter& BlobStreamWriter::operator<<(const PooledBlob& blob)
{
    return (*this << blob.get());
}

BlobStreamWriter& BlobStreamWriter::operator<<(const PooledString& blob)
{
    return (*this << blob.getBytes());
}

BlobStreamWriter& BlobStreamWriter::operator<<(const std::string& blob)
{
    ArrayView<uint8_t> bytes((const uint8_t*)blob.data(), int(blob.size() * sizeof(blob[1])));
    return (*this << bytes);
}


bool BlobStreamReader::operator>>(MutableArrayView<uint8_t> outBlob)
{
    auto maybeBlob = viewNextBlob();
    ArrayView<uint8_t> blob;
    if (maybeBlob.tryGet(blob)) {
        if (blob.size() == outBlob.size()) {
            outBlob.copyFrom(blob);
            return true;
        }
    }
    return false;
}

bool BlobStreamReader::operator>>(std::string& outBlob)
{
    auto maybeBlob = viewNextBlob();
    ArrayView<uint8_t> blob;
    if (maybeBlob.tryGet(blob)) {
        if (blob.size() > 0) {
            outBlob = std::string((const char*)&blob.first(), (size_t)blob.size());
        }
        else {
            outBlob = "";
        }
        return true;
    }
    return false;
}

Optional<ArrayView<uint8_t>> BlobStreamReader::viewNextBlob()
{
    if (m_data.size() < sizeof(int)) {
        return Nothing();
    }

    int blobSize = -1;
    memcpy(&blobSize, &m_data.first(), sizeof(blobSize));
    if (blobSize < 0) { return Nothing(); }

    int endOffset = sizeof(int) + blobSize;
    if (endOffset > m_data.size()) {
        return Nothing();
    }

    auto view = m_data.subView(sizeof(int), blobSize);
    m_data = m_data.subView(endOffset, m_data.size() - endOffset);
    return view;
}
