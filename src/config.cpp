#include <iostream>
#include <fstream>
#include <memory>

#include "config.h"

namespace taoetw {

bool Config::load(const std::wstring& file_)
{
    _file = file_;
    _obj = json11::Json::object {};

    std::ifstream file(file_, std::ios::in | std::ios::binary | std::ios::ate);

    if(file.is_open()) {
        std::streampos size = file.tellg();
        std::unique_ptr<char[]> jsonstr(new char[(unsigned int)size+1]);

        file.seekg(0, std::ios::beg);
        file.read(jsonstr.get(), size);
        jsonstr.get()[(int)size] = '\0';
        file.close();

        std::string errmsg;
        _obj = json11::Json::parse(jsonstr.get(), errmsg);
        if(!_obj.is_object()) {
            _obj = json11::Json::object {};
        };
    }

    return false;
}

bool Config::save()
{
    std::ofstream file(_file, std::ios::out | std::ios::binary);

    if(file.is_open()) {
        auto str = _obj.dump();
        file.write(str.c_str(), str.size());
        file.close();
    }

    return true;
}

const json11::Json& Config::operator[](size_t i)
{
    return _obj[i];
}

const json11::Json& Config::operator[](const char* k)
{
    return _obj[k];
}

std::wstring Config::ws(const std::string& s)
{
    return (const wchar_t*)U8U16Cvt().from_bytes(s).c_str();
}

std::string Config::us(const std::wstring& s)
{
    return (const char*)U8U16Cvt().to_bytes((const unsigned short*)s.c_str()).c_str();
}

}
