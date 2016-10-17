#pragma once

#include <cassert>

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

    json11::Json& operator()(const char* k, json11::Json v)
    {
        auto& map = const_cast<json11::Json::object&>(_obj.object_items());
        return map[k] = v;
    }

    json11::Json root()
    {
        return _obj;
    }

    json11::Json::object& obj(const json11::Json& o)
    {
        assert(o.is_object());
        return const_cast<json11::Json::object&>(o.object_items());
    }

    json11::Json::array& arr(const json11::Json& a)
    {
        assert(a.is_array());
        return const_cast<json11::Json::array&>(a.array_items());
    }

    const json11::Json& ensure_object(const json11::Json& o, const char* k)
    {
        assert(o.is_object());

        std::string err;

        if (!o.has_shape({ {k, json11::Json::OBJECT} }, err)) {
            obj(o)[k] = json11::Json::object{};
        }

        return o[k];
    }

    const json11::Json& ensure_array(const json11::Json& o, const char* k)
    {
        assert(o.is_array());

        std::string err;

        if (!o.has_shape({ {k, json11::Json::ARRAY} }, err)) {
            obj(o)[k] = json11::Json::array{};
        }

        return o[k];
    }

protected:
    std::wstring _file;
    json11::Json _obj;

};

extern Config& g_config;

}
