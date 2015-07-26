#pragma once

#include <string>
#include <vector>
#include <map>

class CommandArgs
{
public:
    CommandArgs(int argc, char* argv[]);

    // Returns the value of the specified option, including possibly an empty string if the option wasn't specified
    std::string getOptionValue(const std::string& optionName, const std::string& defaultValue = "") const;

    // Returns the value of the specified option or displays an error message if none provided.
    std::string expectOptionValue(const std::string& optionName) const;

    // Returns true if the option was specified with no value, false if no such option exists, or displays an error if the option had a value
    bool hasSwitchEnabled(const std::string& optionName) const; 

    // Returns how many unnamed arguments (initial arguments with no "-" prefix) were parsed
    int getUnnamedArgCount() const;

    // Gets the nth unnamed argument
    std::string getUnnamedArg(int index) const;

    // First the first unnamed argument and removes it, so the next call to this will return the second, etc.
    std::string popUnnamedArg();

private:
    void parse(std::vector<std::string>&& words);

    std::map<std::string, std::string> m_namedArgs;
    std::vector<std::string> m_unnamedArgs;
    int m_popCount;
};

