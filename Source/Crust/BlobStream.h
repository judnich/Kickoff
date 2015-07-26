#pragma once
#include <cstdint>
#include "Array.h"
#include "Algebraic.h"
#include "PooledString.h"
#include "PooledBlob.h"


class BlobStreamWriter
{
public:
    void reserve(size_t bytes);
    ArrayView<uint8_t> data() const { return ArrayView<uint8_t>(m_data); }

    BlobStreamWriter& operator<< (ArrayView<uint8_t> blob);
    BlobStreamWriter& operator<< (const std::string& blob);
    BlobStreamWriter& operator<< (const PooledString& blob);
    BlobStreamWriter& operator<< (const PooledBlob& blob);

    template<class T>
    BlobStreamWriter& operator<< (ArrayView<T> blob)
    {
        ArrayView<uint8_t> bytes(&blob.first(), blob.size() * sizeof(T));
        return (*this << bytes);
    }

    template<class T>
    BlobStreamWriter& operator<< (const T& blob)
    {
        ArrayView<uint8_t> bytes((uint8_t*)&blob, sizeof(T));
        return (*this << bytes);
    }

private:
    std::vector<uint8_t> m_data;
};


class BlobStreamReader
{
public:
    BlobStreamReader() {}
    BlobStreamReader(ArrayView<uint8_t> data) : m_data(data) {}

    bool operator>> (MutableArrayView<uint8_t> outBlob);
    bool operator>> (std::string& outBlob);

    template<class T>
    bool operator>> (std::vector<T>& outBlob)
    {
        auto maybeBlob = viewNextBlob();
        ArrayView<uint8_t> blob;
        if (maybeBlob.tryGet(blob)) {
            outBlob = std::vector<T>((size_t)blob.size());
            memcpy(outBlob.data(), &blob.first(), sizeof(T) * (size_t)blob.size());
            return true;
        }
        return false;
    }

    template<class T>
    bool operator>> (MutableArrayView<T> outBlob)
    {
        MutableArrayView<uint8_t> bytes(&outBlob.first(), outBlob.size() * sizeof(T));
        return (*this >> bytes);
    }

    template<class T>
    bool operator>> (T& outBlob)
    {
        MutableArrayView<uint8_t> bytes((uint8_t*)&outBlob, sizeof(T));
        return (*this >> bytes);
    }

    bool operator>> (PooledString& outBlob)
    {
        std::string blobStr;
        if (!(*this >> blobStr)) { return false; }
        outBlob = PooledString(std::move(blobStr));
        return true;
    }

    bool operator>> (PooledBlob& outBlob)
    {
        std::vector<uint8_t> blobVec;
        if (!(*this >> blobVec)) { return false; }
        outBlob = PooledBlob(std::move(blobVec));
        return true;
    }

    Optional<ArrayView<uint8_t>> viewNextBlob();

private:
    ArrayView<uint8_t> m_data;
};
