// C++ algebraic data type template library
#pragma once
#include <functional>
#include <assert.h>
#include <memory>

// Non-nullable sum type / disjoint union. Kind of like std::pair but only one is set at any given time, and it's impossible to 'get' the wrong one.
// Note that it is intentional that you can implicitly assign an Either<T1, T2> to either of T1 or T2 values without manually
// specifying the constructor, e.g. "Either<T1, T2> x = T1();" Getting is more complex though since you need to realize that
// any attempt to access either T1 or T2 could fail. There are several methods offered to do this safely.
// If you want/need to modify the contained value in-place, use the unwrap method.

template<class T1, class T2>
class Either
{
public:
    Either(const T1& val)
    {
        new (asT1()) T1(val); // placement new with deep copy
        m_hasFirst = true;
    }

    Either(const T2& val)
    {
        new (asT2()) T2(val); // placement new with deep copy
        m_hasFirst = false;
    }

    ~Either()
    {
        if (m_hasFirst) {
            asT1()->~T1();
        }
        else {
            asT2()->~T2();
        }
    }

    bool hasFirst() const { return m_hasFirst; }

    void unwrap(std::function<void(const T1&)> handleFirst, std::function<void(const T2&)> handleSecond) const
    {
        if (m_hasFirst) {
            handleFirst(*asT1());
        }
        else {
            handleSecond(*asT2());
        }
    }

    void unwrap(std::function<void(T1&)> handleFirst, std::function<void(T2&)> handleSecond)
    {
        if (m_hasFirst) {
            handleFirst(*asT1());
        }
        else {
            handleSecond(*asT2());
        }
    }

    bool tryGet(T1& out) const
    {
        if (m_hasFirst) {
            out = *asT1();
            return true;
        }
        return false;
    }

    bool tryGet(T2& out) const
    {
        if (!m_hasFirst) {
            out = *asT2();
            return true;
        }
        return false;
    }

    Either<T1, T2>& operator= (const Either<T1, T2>& other)
    {
        if (&other == this) { return *this; }
        this->~Either();
        new (this) Either<T1, T2>(other);
        return *this;
    }

    Either<T1, T2>& operator= (Either<T1, T2>&& other)
    {
        this->~Either();
        new (this) Either<T1, T2>(std::move(other));
        return *this;
    }

    Either(const Either<T1, T2>& other)
    {
        // Make a deep copy into our owned storage via the copy constructor
        if (other.m_hasFirst) {
            new (asT1()) T1(*other.asT1());
        }
        else {
            new (asT2()) T2(*other.asT2());
        }
        m_hasFirst = other.m_hasFirst;
    }

    Either(Either<T1, T2>&& other)
    {
        // Make a shallow copy into our owned storage via simple memcpy
        if (other.m_hasFirst) {
            memcpy(m_data, other.m_data, sizeof(T1));
        }
        else {
            memcpy(m_data, other.m_data, sizeof(T2));
        }
        m_hasFirst = other.m_hasFirst;
    }

    // For cases where default constructor is required
    Either()
    {
        new (asT2()) T2();
        m_hasFirst = false;
    }

private:
    char m_data[sizeof(T1) > sizeof(T2) ? sizeof(T1) : sizeof(T2)];
    bool m_hasFirst;

    T1* asT1() { return reinterpret_cast<T1*>(m_data); }
    const T1* asT1() const { return reinterpret_cast<const T1*>(m_data); }
    T2* asT2() { return reinterpret_cast<T2*>(m_data); }
    const T2* asT2() const { return reinterpret_cast<const T2*>(m_data); }
};

// Reference implementation (less efficient)
/*template<class T1, class T2>
class Either
{
public:
	Either(const T1& val)
		: m_t1(std::make_unique<T1>(val))
	{
		m_hasFirst = true;
	}

	Either(const T2& val)
		: m_t2(std::make_unique<T2>(val))
	{
		m_hasFirst = false;
	}

	Either(T1&& val)
		: m_t1(std::make_unique<T1>(val))
	{
		m_hasFirst = true;
	}

	Either(T2&& val)
		: m_t2(std::make_unique<T2>(val))
	{
		m_hasFirst = false;
	}

	void operator= (const Either& other)
	{
		m_hasFirst = other.m_hasFirst;
		if (other.m_hasFirst) {
			m_t1.reset(new T1(*other.m_t1));
			m_t2.reset();
		}
		else {
			m_t2.reset(new T2(*other.m_t2));
			m_t1.reset();
		}
	}

	Either(const Either& other)
	{
		m_hasFirst = other.m_hasFirst;
		if (other.m_hasFirst) {
			m_t1.reset(new T1(*other.m_t1));
		}
		else {
			m_t2.reset(new T2(*other.m_t2));
		}
	}

	~Either()
	{
	}

	bool hasFirst() const { return m_hasFirst; }

	void unwrap(std::function<void(const T1&)> handleFirst, std::function<void(const T2&)> handleSecond) const
	{
		if (m_hasFirst) {
			handleFirst(*m_t1);
		}
		else {
			handleSecond(*m_t2);
		}
	}

	void unwrap(std::function<void(T1&)> handleFirst, std::function<void(T2&)> handleSecond)
	{
		if (m_hasFirst) {
			handleFirst(*m_t1);
		}
		else {
			handleSecond(*m_t2);
		}
	}

	bool tryGet(T1& out) const
	{
		if (m_hasFirst) {
			out = *m_t1;
			return true;
		}
		return false;
	}

	bool tryGet(T2& out) const
	{
		if (!m_hasFirst) {
			out = *m_t2;
			return true;
		}
		return false;
	}

private:
	std::unique_ptr<T1> m_t1;
	std::unique_ptr<T2> m_t2;
	bool m_hasFirst;
};*/


// Option type -- essentially just syntactic sugar wrapper for Either<T, Nothing>. This means you can assign
// an Option<T> to either a T or a Nothing and it should compile without the need to manually cast the RHS to an Optional.
// If you want/need to modify the contained value (if any) in-place, use the tryUnwrap method.
struct Nothing { };

template<class T>
class Optional
{
public:
    Optional() : m_value(Nothing()) {}
    Optional(Nothing) : m_value(Nothing()) {}
    Optional(const T& val) : m_value(val) {}
    Optional(T&& val) : m_value(val) {}

	void operator= (const Optional& other)
	{
		m_value = other.m_value;
	}

    bool hasValue() const { return m_value.hasFirst(); }

    bool tryUnwrap(std::function<void(const T&)> handleValue) const
    {
        m_value.get(handleValue, [](const Nothing&){});
        return hasValue();
    }

    bool tryUnwrap(std::function<void(T&)> handleValue)
    {
        m_value.get(handleValue, [](const Nothing&){});
        return hasValue();
    }

    bool tryGet(T& out) const
    {
        return m_value.tryGet(out);
    }

    T orDefault(const T& defaultVal = T()) const
    {
        T val;
        if (tryGet(val)) {
            return std::move(val);
        }
        return defaultVal;
    }

    T orFail(const std::string& errorMessage)
    {
        T val;
        if (tryGet(val)) {
            return std::move(val);
        }
        fail(errorMessage);
    }

    Either<T, Nothing> toEither() { return m_value; }

private:
    Either<T, Nothing> m_value;
};

