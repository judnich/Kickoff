// C++ algebraic data type template library
#pragma once
#include <functional>
#include <assert.h>
#include <memory>

struct Nothing { };

template<class T>
class Optional
{
public:
    Optional() : m_hasValue(false) {}
    Optional(Nothing) : m_hasValue(false) {}
    Optional(const T& val) : m_value(val), m_hasValue(true) {}
    Optional(T&& val) : m_value(val), m_hasValue(true) {}

    void operator= (const Optional& other)
    {
        m_value = other.m_value;
        m_hasValue = other.m_hasValue;
    }

    bool hasValue() const { return m_hasValue; }

    T* ptrOrNull()
    {
        if (m_hasValue) { return &m_value; }
        return nullptr;
    }

    const T* ptrOrNull() const
    {
        if (m_hasValue) { return &m_value; }
        return nullptr;
    }

    T orDefault(const T& defaultVal = T()) const
    {
        if (m_hasValue) { return m_value; }
        return defaultVal;
    }

    const T& refOrFail(const std::string& errorMessage) const
    {
        if (!m_hasValue) { fail(errorMessage); }
        return m_value;
    }

    T& refOrFail(const std::string& errorMessage)
    {
        if (!m_hasValue) { fail(errorMessage); }
        return m_value;
    }

    T orFail(const std::string& errorMessage) const
    {
        if (!m_hasValue) { fail(errorMessage); }
        return m_value;
    }

    T&& moveContentsOrFail(const std::string& errorMessage)
    {
        if (!m_hasValue) { fail(errorMessage); }
        m_hasValue = false;
        return std::move(m_value);
    }

private:
    T m_value;
    bool m_hasValue;
};

