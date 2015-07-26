#pragma once
#include <string>
#include <cstdint>
#include "Array.h"
#include "Algebraic.h"

std::wstring stringToWstring(const std::string& s);

Optional<uint64_t> hexStringToUint64(const std::string& str); // returns nothing if nonhex char or overflow
std::string toHexString(ArrayView<uint8_t> data);

template<class T>
std::string toHexString(const T& data)
{
    return toHexString(ArrayView<uint8_t>((uint8_t*)&data, (int)sizeof(data)));
}

std::vector<std::string> splitString(const std::string& str, const std::string& separators, bool allowBlank = true);

uint64_t hashData(ArrayView<uint8_t> data);

Optional<std::vector<uint8_t>> readFileData(const std::string& filename);
bool writeFileData(ArrayView<uint8_t> data, const std::string& filename);

std::string getFileExtension(const std::string& path);

int parseInt(const std::string& str);

std::string getMachineName();

bool makeDirectory(const std::string& dirPath);
bool deleteDirectory(const std::string& dirPath, bool recursive = false);
bool deleteFile(const std::string& filePath);