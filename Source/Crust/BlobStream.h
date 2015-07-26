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

    template<class T>
    BlobStreamWriter& operator<< (const std::vector<T>& blob)
    {
        *this << (int)blob.size();
        return (*this << blob);
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
    bool operator>> (PooledString& outBlob);
    bool operator>> (PooledBlob& outBlob);

    template<class T>
    bool operator>> (std::vector<T>& outBlob)
    {
        int size = 0;
        if (!(*this >> size)) { return false; }
        if (size < 0) { return false; }
        outBlob.resize((size_t)size);
        if (size == 0) { return true; }
        MutableArrayView<T> outBlobView(outBlob);
        if (!(*this >> outBlobView)) { return false; }
        return true;
    }

    template<class T>
    bool operator>> (MutableArrayView<T> outBlob)
    {
        MutableArrayView<uint8_t> bytes((uint8_t*)&outBlob.first(), outBlob.size() * sizeof(T));
        return (*this >> bytes);
    }

    template<class T>
    bool operator>> (T& outBlob)
    {
        MutableArrayView<uint8_t> bytes((uint8_t*)&outBlob, sizeof(T));
        return (*this >> bytes);
    }

    bool hasMore() const { return m_data.size() > 0; }

private:
    ArrayView<uint8_t> m_data;
};
