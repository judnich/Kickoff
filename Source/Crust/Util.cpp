#include "Util.h"
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include "External/MurmurHash2_64.h"


#ifdef _WIN32
#include <windows.h>
#define _NO_OLDNAMES
#endif


std::wstring stringToWstring(const std::string& s)
{
    std::wstring temp(s.length(), L' ');
    std::copy(s.begin(), s.end(), temp.begin());
    return temp;
}

static const char gIntToHex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

std::string toHexString(ArrayView<uint8_t> data)
{
    std::string hex;
    for (int i = data.size() - 1; i >= 0; --i) {
        uint8_t v = data[i];
        uint8_t low = v & 0xF;
        uint8_t high = (v >> 4) & 0xF;
        hex += gIntToHex[high];
        hex += gIntToHex[low];
    }
    return hex;
}

std::vector<std::string> splitString(const std::string& str, const std::string& separators, bool allowBlank)
{
    std::vector<std::string> words;
    std::string word;

    for (size_t i = 0; i < str.size(); ++i) {
        char ch = str[i];
        
        bool isSeparator = false;
        for (wchar_t sepCh : separators) {
            if (ch == sepCh) { isSeparator = true; break; }
        }

        if (isSeparator) {
            if (allowBlank || !word.empty()) {
                words.push_back(std::move(word));
                word = "";
            }
        }
        else {
            word += ch;
        }
    }

    if (allowBlank || !word.empty()) {
        words.push_back(std::move(word));
        word = "";
    }

    return std::move(words);
}

Optional<uint64_t> hexStringToUint64(const std::string& str)
{
    uint64_t val = 0;
    if (str.size() > 16) { return Nothing(); }

    for (auto ch : str) {
        uint8_t chVal;
        if (ch >= '0' && ch <= '9') {
            chVal = (ch - '0');
        }
        else if (ch >= 'a' && ch <= 'f') {
            chVal = (ch - 'a') + 10;
        }
        else if (ch >= 'A' && ch <= 'F') {
            chVal = (ch - 'A') + 10;
        }
        else {
            return Nothing();
        }

        val <<= 4;
        val |= chVal;
    }

    return val;
}

uint64_t hashData(ArrayView<uint8_t> data)
{
    return MurmurHash64A(&data.first(), data.size(), 123);
}

Optional<std::vector<uint8_t>> readFileData(const std::string& filename)
{
    FILE* f;
    errno_t result = fopen_s(&f, filename.c_str(), "rb");
    if (result != 0 || !f) { return Nothing(); }

    fseek(f, 0, SEEK_END);
    size_t fsize = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> data;
    data.resize(fsize);
    if (fread(data.data(), fsize, 1, f) != 1) {
        fclose(f);
        return Nothing();
    }

    fclose(f);
    return std::move(data);
}

bool writeFileData(ArrayView<uint8_t> data, const std::string& filename)
{
    FILE* f;
    errno_t result = fopen_s(&f, filename.c_str(), "wb");
    if (result != 0 || !f) { return false; }

    if (fwrite(&data[0], data.size(), 1, f) != 1) {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

std::string getFileExtension(const std::string& path)
{
    for (int i = (int)path.size() - 1; i >= 0; --i) {
        char ch = path[i];
        if (ch == '.') {
            return path.substr(i);
        }
        else if (ch == '/' || ch == '\\') {
            return "";
        }
    }
    return "";
}

int parseInt(const std::string& str)
{
    return atoi(str.c_str());
}

std::string getMachineName()
{
    TCHAR nameBuf[MAX_COMPUTERNAME_LENGTH + 2];
    DWORD nameBufSize;

    nameBufSize = sizeof nameBuf - 1;
    if (GetComputerName(nameBuf, &nameBufSize) == TRUE) {
        char nameBuf8[MAX_COMPUTERNAME_LENGTH + 2];
        size_t len = wcslen(nameBuf);
        for (size_t i = 0; i < len + 1; ++i) {
            nameBuf8[i] = (char)nameBuf[i];
        }
        return std::string(nameBuf8);
    }

    return "";
}

bool makeDirectory(const std::string& dirPath)
{
    return CreateDirectory(stringToWstring(dirPath).c_str(), NULL) == TRUE;
}

bool deleteDirectory(const std::string& dirPath, bool recursive)
{
    auto dirPathW = stringToWstring(dirPath + '\0');

    if (!recursive) {
        return RemoveDirectory(dirPathW.c_str()) == TRUE;
    }
    else {
        SHFILEOPSTRUCTW fileOperation;
        fileOperation.wFunc = FO_DELETE;
        fileOperation.pFrom = dirPathW.c_str();
        fileOperation.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;

        int result = ::SHFileOperationW(&fileOperation);
        if (result != 0) {
            return false;
        }
        return true;
    }
}

bool deleteFile(const std::string& filePath)
{
    return DeleteFile(stringToWstring(filePath).c_str()) == TRUE;
}
