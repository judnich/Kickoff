#pragma once

#include <exception>
#include <string>
#include "Util.h"

class FailFastException : public std::exception
{
public:
    FailFastException(const char* filename, int line)
        : m_file(filename), m_line(line)
    {}

    ~FailFastException() throw() {}

    const char* what() const throw();

private:
    std::string m_file;
    int m_line;
};

#define FAILFAST() { throw new FailFastException(__FILE__, __LINE__); }

void fail(const std::string& message);
void printError(const std::string& message);
void printWarning(const std::string& message);

inline void runtimeAssert(bool condition, const std::string& message) { if (!condition) { fail(message); } }
