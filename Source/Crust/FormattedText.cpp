#include "FormattedText.h"
#include "../external/rlutil.h"


ColoredString::ColoredString(const std::string& str, TextColor color)
{
    assert(str.size() < (size_t)INT_MAX);
    chars.resize(str.size());
    for (size_t i = 0; i < str.size(); ++i)
    {
        chars[i] = ColoredChar(str[i], color);
    }
}

void ColoredString::print(bool paragraph, int leftMargin, int rightMargin) const
{
    if (chars.size() == 0) {
        return;
    }
    assert((leftMargin == 0 && rightMargin == 0) || paragraph == true);

    TextColor lastColor = chars[0].color;
    rlutil::setColor((int)lastColor);
    if (paragraph) {
        printf("\r");
    }

    int width = rlutil::tcols();
    int col = 0;

    for (auto ch : chars) {
        // Make sure the left margin is inserted
        while (col < leftMargin) {
            printf(" ");
            col++;
        }

        // Wrap around to the next line when crossing the right margin
        bool wrapped = false;
        if (col >= width - rightMargin) {
            printf("\n");
            wrapped = true;
            col = 0;
            while (col < leftMargin) {
                printf(" ");
                col++;
            }
        }

        // Update console foreground color
        if (ch.color != lastColor) {
            rlutil::setColor((int)ch.color);
            lastColor = ch.color;
        }

        // Skip writing a single extra space after auto-inserting a new column
        if (ch.ch != L' ' || !wrapped)
        {
            // Print the character and update the column count
            wprintf(L"%c", ch.ch);
            if (ch.ch == L'\n' || ch.ch == L'\r') {
                col = 0;
            }
            else {
                col++;
            }
        }
    }

    rlutil::setColor((int)TextColor::Gray);
    if (paragraph && col != 0) {
        printf("\n");
    }
}

void ColoredString::operator+=(ColoredChar ch)
{
    chars.push_back(ch);
}

void ColoredString::operator+=(const ColoredString& other)
{
    for (auto ch : other.chars) {
        chars.push_back(ch);
    }
}

ColoredString ColoredString::operator+(const ColoredString& other) const
{
    ColoredString copy(*this);
    copy += other;
    return std::move(copy);
}


void TextBlock::print(int leftMargin, int rightMargin) const
{
    text.print(true, leftMargin, rightMargin);
}


TextContainer::TextContainer(int hMargin, int topMargin, ITextNode::Ptr&& firstItem)
{
    this->leftMargin = hMargin;
    this->rightMargin = hMargin;
    this->topMargin = topMargin;
    this->bottomMargin = 0;
    if (firstItem) {
        (*this) += std::move(firstItem);
    }
}

TextContainer::TextContainer(int lMargin, int rMargin, int tMargin, int bMargin, ITextNode::Ptr&& firstItem)
{
    this->leftMargin = lMargin;
    this->rightMargin = rMargin;
    this->topMargin = tMargin;
    this->bottomMargin = bMargin;
    if (firstItem) {
        (*this) += std::move(firstItem);
    }
}

void TextContainer::print(int addLeftMargin, int addRightMargin) const
{
    for (int i = 0; i < topMargin; ++i) { printf("\n"); }

    int thisLeftMargin = leftMargin + addLeftMargin;
    int thisRightMargin = rightMargin + addRightMargin;

    for (const auto& val : elements) {
        val->print(thisLeftMargin, thisRightMargin);
    }

    for (int i = 0; i < bottomMargin; ++i) { printf("\n"); }
}


TextHeader::TextHeader(ColoredString&& text, ColoredChar rulerChar)
    : text(std::move(text)), rulerChar(rulerChar)
{}

void TextHeader::print(int leftMargin, int rightMargin) const
{
    for (int i = 0; i < leftMargin; ++i) {
        printf(" ");
    }
    int totalRulerChars = rlutil::tcols() - (leftMargin + rightMargin + 2 + text.size());
    int leftRulerChars = totalRulerChars / 2;
    int rightRulerChars = totalRulerChars - leftRulerChars;

    rlutil::setColor((int)rulerChar.color);
    for (int i = 0; i < leftRulerChars; ++i) {
        wprintf(L"%c", rulerChar.ch);
    }

    printf(" ");
    text.print(false, 0, 0);
    printf(" ");

    rlutil::setColor((int)rulerChar.color);
    for (int i = 0; i < rightRulerChars; ++i) {
        wprintf(L"%c", rulerChar.ch);
    }

    if (leftMargin + 2 + leftRulerChars + rightRulerChars + text.size() != rlutil::tcols()) {
        printf("\n");
    }
    rlutil::setColor((int)TextColor::Gray);
}

