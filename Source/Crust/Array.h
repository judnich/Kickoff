#pragma once
#include <assert.h>
#include <vector>


template<class T, int N>
class StaticArray
{
public:
    StaticArray(const T& initialValues = T())
    {
        MutableArrayView<T>(*this).fill(initialValues);
    }

    int size() const { return N; }
    T* data() { return m_dataAllocation; }
    const T* data() const { return m_dataAllocation; }
    
    bool operator== (const StaticArray<T, N>& other) const { return ArrayView<T>(*this) == ArrayView<T>(other); }
    bool operator!= (const StaticArray<T, N>& other) const { return !(ArrayView<T>(*this) == ArrayView<T>(other)); }

private:
    T m_dataAllocation[N];
};


template<class T>
class ArrayView
{
public:
    ArrayView() : m_data(nullptr), m_size(0) {}

    explicit ArrayView(const T *firstElement, int numElements)
        : m_data(firstElement)
        , m_size(numElements)
    {}

    ArrayView(const std::vector<T> &vec)
    {
        m_data = vec.data();
        assert(vec.size() < (size_t)INT_MAX);
        m_size = (int)vec.size();
    }

    template <int N>
    ArrayView(const StaticArray<T, N>& a)
        : m_data(a.data())
        , m_size(a.size())
    {}

    int size() const { return m_size; }
    const T& operator[] (int index) const { assert(index >= 0 && index < m_size); return m_data[index]; }
    
    bool operator== (ArrayView other) const
    {
        if (m_size != other.m_size) { return false; }
        for (int i = 0; i < m_size; ++i) {
            if (m_data[i] != other.m_data[i]) { return false; }
        }
        return true;
    }

    bool operator!= (ArrayView other) const { return !(*this == other); }

    const T& first() const { return (*this)[0]; }
    const T& last() const { return (*this)[size() - 1]; }

    std::vector<T> clone() const
    {
        std::vector<T> vec(m_size);
        memcpy(vec.data(), m_data, sizeof(T) * m_size);
        return std::move(vec);
    }

    ArrayView<T> subView(int start, int count) const
    {
        ArrayView sub = *this;
        sub.applySubView(start, count);
        return std::move(sub);
    }

    class ConstIterator
    {
    public:
        ConstIterator(ArrayView<T> data, int pos) : m_data(data), m_pos(pos) {}
        const T& operator* () const { return m_data[m_pos]; }
        ConstIterator& operator++ () { m_pos++; return *this; }
        bool operator== (const ConstIterator& other) const { return m_pos == other.m_pos && m_data == other.m_data; }
        bool operator!= (const ConstIterator& other) const { return !(*this == other); }

    private:
        ArrayView<T> m_data;
        int m_pos;
    };

    ConstIterator begin() const { return ConstIterator(*this, 0); }
    ConstIterator end() const { return ConstIterator(*this, size()); }

protected:
    const T* m_data;
    int m_size;

    void applySubView(int start, int count)
    {
        assert(count >= 0 && start >= 0 && start + count <= m_size);
        m_data += start;
        m_size = count;
    }
};

// Implement std::hash for ArrayView so it can be used in hash maps, sets, etc
namespace std {
    template <class T> struct hash<ArrayView<T>>
    {
        size_t operator()(ArrayView<T> x) const { return hashData(ArrayView<uint8_t>(x.data(), x.size() * sizeof(T))); }
    };
}


template<class T>
class MutableArrayView : public ArrayView<T>
{
public:
    MutableArrayView() : ArrayView() {}

    explicit MutableArrayView(T* firstElement, int numElements)
        : ArrayView(firstElement, numElements)
    {}

    MutableArrayView(std::vector<T>& vec)
    {
        m_data = vec.data();
        assert(vec.size() < (size_t)INT_MAX);
        m_size = (int)vec.size();
    }
    
    template <int N>
    MutableArrayView(StaticArray<T, N>& a)
        : ArrayView(a.data(), a.size())
    {}

    T& operator[] (int index) { return const_cast<T&>(static_cast<ArrayView>(*this)[index]); }
    const T& operator[] (int index) const { assert(index >= 0 && index < m_size); return m_data[index]; }

    T& first() { return (*this)[0]; }
    T& last() { return (*this)[size() - 1]; }
    const T& first() const { return (*this)[0]; }
    const T& last() const { return (*this)[size() - 1]; }

    MutableArrayView<T> subView(int start, int count)
    {
        MutableArrayView sub = *this;
        sub.applySubView(start, count);
        return std::move(sub);
    }

    void copyFrom(ArrayView<T> other)
    {
        assert(this->size() == other.size());
        for (int i = 0; i < size(); ++i)
        {
            (*this)[i] = other[i];
        }
    }

    void fill(const T& val)
    {
        for (int i = 0; i < size(); ++i)
        {
            (*this)[i] = val;
        }
    }

    class Iterator
    {
    public:
        Iterator(MutableArrayView<T> data, int pos) : m_data(data), m_pos(pos) {}
        T& operator* () { return m_data[m_pos]; }
        Iterator& operator++ () { m_pos++; return *this; }
        bool operator== (const Iterator& other) const { return m_pos == other.m_pos && m_data == other.m_data; }
        bool operator!= (const Iterator& other) const { return !(*this == other); }

    private:
        MutableArrayView<T> m_data;
        int m_pos;
    };

    Iterator begin() const { return Iterator(*this, 0); }
    Iterator end() const { return Iterator(*this, size()); }
};
