#include "CommandArgs.h"
#include "Util.h"
#include "Crust/Error.h"


CommandArgs::CommandArgs(int argc, char* argv[])
    : m_popCount(0)
{
    std::vector<std::string> words;
    for (int i = 1; i < argc; ++i) {
        words.push_back(std::string(argv[i]));
    }
    parse(std::move(words));
}

void CommandArgs::parse(std::vector<std::string>&& words)
{
    std::string optionName;
    for (auto& word : words) {
        if (word.size() > 1 && word[0] == L'-') {
            optionName = word.substr(1);
            m_namedArgs[optionName] = "";
        }
        else {
            if (optionName == "") {
                m_unnamedArgs.push_back(word);
            }
            else {
                auto& optionValue = m_namedArgs[optionName];
                if (optionValue == "") {
                    optionValue = word;
                }
                else {
                    optionValue += " " + word;
                }
            }
        }
    }
}

int CommandArgs::getUnnamedArgCount() const
{
    return (int)m_unnamedArgs.size() - m_popCount;
}

std::string CommandArgs::getUnnamedArg(int index) const
{
    index += m_popCount;
    if (index >= (int)m_unnamedArgs.size()) {
        printError("Expected at least " + std::to_string(index + 1) + " initial command-line argument(s).");
        exit(-1);
    }

    return m_unnamedArgs[index];
}

std::string CommandArgs::popUnnamedArg()
{
    const std::string& val = getUnnamedArg(0);
    m_popCount++;
    return val;
}

std::string CommandArgs::getOptionValue(const std::string& optionName, const std::string& defaultValue) const
{
    auto it = m_namedArgs.find(optionName);
    if (it == m_namedArgs.end()) { return defaultValue; }
    return it->second;
}

std::string CommandArgs::expectOptionValue(const std::string& optionName) const
{
    std::string val = getOptionValue(optionName);
    if (val == "") {
        printError("Expected command-line option \"-" + optionName + "\" to have a non-empty value.");
        exit(-1);
    }
    return val;
}

bool CommandArgs::hasSwitchEnabled(const std::string& optionName) const
{
    auto it = m_namedArgs.find(optionName);
    if (it == m_namedArgs.end()) { return false; }

    if (it->second != "") {
        printError("Expected command-line switch \"-" + optionName +
            "\" to either exist or not with no value; but found it followed by the text: \"" +
            it->second + "\"!");
        exit(-1);
    }

    return true;
}
