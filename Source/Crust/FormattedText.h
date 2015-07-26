#pragma once
#include <string>
#include <vector>
#include <memory>
#include <wchar.h>
#include "Algebraic.h"
#include "Array.h"

enum class TextColor
{
    Black,
    Blue,
    Green,
    Cyan,
    Red,
    Magenta,
    Yellow,
    Gray,
    DarkGray,
    LightBlue,
    LightGreen,
    LightCyan,
    LightRed,
    LightMagenta,
    LightYellow,
    White
};

struct ColoredChar
{
    char ch;
    TextColor color;

    ColoredChar(char ch = '\0', TextColor color = TextColor::Gray)
        : ch(ch), color(color) {}
};

struct ColoredString
{
    std::vector<ColoredChar> chars;

    ColoredString(const std::string& str, TextColor color = TextColor::Gray);

    int size() const { return (int)chars.size(); }
    void print(bool paragraph = false, int leftMargin = 0, int rightMargin = 0) const;

    void operator+= (const ColoredString& other);
    void operator+= (ColoredChar ch);
    ColoredString operator+ (const ColoredString& other) const;
};

class ITextNode
{
public:
    virtual ~ITextNode() {}
    typedef std::unique_ptr<ITextNode> Ptr;
    virtual void print(int leftMargin, int rightMargin) const = 0;
    void print() const { print(0, 0); }
};

class TextContainer : public ITextNode
{
public:
    typedef std::unique_ptr<TextContainer> Ptr;

    TextContainer(int hMargin = 0, int topMargin = 0, ITextNode::Ptr&& firstItem = ITextNode::Ptr());
    TextContainer(int lMargin, int rMargin, int tMargin, int bMargin, ITextNode::Ptr&& firstItem = ITextNode::Ptr());

    static TextContainer::Ptr make(int hMargin = 0, int topMargin = 0, ITextNode::Ptr&& firstItem = ITextNode::Ptr())
    {
        return std::make_unique<TextContainer>(hMargin, topMargin, std::move(firstItem));
    }

    static TextContainer::Ptr make(int lMargin, int rMargin, int tMargin, int bMargin, ITextNode::Ptr&& firstItem = ITextNode::Ptr())
    {
        return std::make_unique<TextContainer>(lMargin, rMargin, tMargin, bMargin, std::move(firstItem));
    }

    void operator+= (ITextNode::Ptr&& item)
    {
        elements.push_back(std::move(item));
    }

    void print(int leftMargin = 0, int rightMargin = 0) const;

    std::vector<ITextNode::Ptr> elements;
    int leftMargin, rightMargin, topMargin, bottomMargin;
};

class TextBlock : public ITextNode
{
public:
    typedef std::unique_ptr<TextBlock> Ptr;

    TextBlock(ColoredString&& str) : text(std::move(str)) {}

    static TextBlock::Ptr make(ColoredString&& str)
    {
        return std::make_unique<TextBlock>(std::move(str));
    }

    static TextBlock::Ptr make(const std::string& str, TextColor color = TextColor::Gray)
    {
        return std::make_unique<TextBlock>(ColoredString(str, color));
    }

    void print(int leftMargin = 0, int rightMargin = 0) const;

    ColoredString text;
};

class TextHeader : public ITextNode
{
public:
    TextHeader(ColoredString&& text, ColoredChar rulerChar);

    static TextHeader::Ptr make(ColoredString&& text, ColoredChar rulerChar)
    {
        return std::make_unique<TextHeader>(std::move(text), rulerChar);
    }

    static TextHeader::Ptr make(const std::string& text, char rulerChar = '-', TextColor textColor = TextColor::LightMagenta, TextColor rulerColor = TextColor::Magenta)
    {
        return std::make_unique<TextHeader>(
            ColoredString(text, textColor),
            ColoredChar(rulerChar, rulerColor)
        );
    }

    ColoredString text;
    ColoredChar rulerChar;

    void print(int leftMargin = 0, int rightMargin = 0) const;
};
