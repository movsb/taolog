#pragma once

#include <json11/json11.hpp>
#include <codecvt>

namespace taoetw {

class Config
{
public:
    typedef std::wstring_convert<std::codecvt_utf8_utf16<unsigned short>, unsigned short> U8U16Cvt;

public:
    Config()
    {}

    bool load(const std::wstring& file);
    bool save();

    const json11::Json& operator[](size_t i);
    const json11::Json& operator[](const char* k);

    static std::wstring ws(const std::string& s);
    static std::string us(const std::wstring& s);

    json11::Json& obj() { return _obj; }

protected:
    std::wstring _file;
    json11::Json _obj;

};

extern Config& g_config;

}
