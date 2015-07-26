#include "Error.h"
#include "FormattedText.h"

const char* FailFastException::what() const throw()
{
    return ("FailFastException at " + m_file + ":" + std::to_string(m_line)).c_str();
}

void fail(const std::string& message)
{
    printError(message);
    FAILFAST();
}

void printError(const std::string& message)
{
    TextContainer doc(2, 2, 1, 1);
    doc += TextHeader::make("Error", '*', TextColor::LightRed, TextColor::LightRed);
    doc += TextBlock::make(ColoredString("ERROR: ", TextColor::LightRed) + ColoredString(message, TextColor::Red));
    doc.print();
}

void printWarning(const std::string& message)
{
    TextContainer doc(2, 2, 1, 1);
    doc += TextBlock::make(ColoredString("WARNING: ", TextColor::LightYellow) + ColoredString(message, TextColor::Yellow));
    doc.print();
}
