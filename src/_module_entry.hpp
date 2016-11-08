#pragma once

namespace taoetw {

struct ModuleEntry
{
    std::wstring    name;
    std::wstring    root;
    bool            enable;
    unsigned char   level;
    GUID            guid;
    std::wstring    guid_str;

    json11::Json to_json() const
    {
        return json11::Json::object {
            {"name",   g_config.us(name)},
            {"root",   g_config.us(root)},
            {"enable", enable},
            {"level",  level},
            {"guid",   g_config.us(guid_str)},
        };
    }

    static ModuleEntry* from_json(const json11::Json& obj)
    {
        ModuleEntry* m = nullptr;

        auto name = obj["name"];
        auto root = obj["root"];
        auto enable = obj["enable"];
        auto level = obj["level"];
        auto guidstr = obj["guid"];
        GUID guid = {};

        if((name.is_string()
            && root.is_string()
            && enable.is_bool()
            && level.is_number()
            && guidstr.is_string())
            && (level >= TRACE_LEVEL_CRITICAL && level <= TRACE_LEVEL_VERBOSE)
            && (!FAILED(::CLSIDFromString(g_config.ws(guidstr.string_value()).c_str(), &guid)))
        )
        {
            m = new ModuleEntry;

            m->name = g_config.ws(name.string_value());
            m->root = g_config.ws(root.string_value());
            m->enable = enable.bool_value();
            m->level = (unsigned char)level.int_value();
            m->guid = guid;
            m->guid_str = g_config.ws(guidstr.string_value());
        }

        return m;
    }
};

struct GUIDLessComparer {
    bool operator()(const GUID& left, const GUID& right) const
    {
        return ::memcmp(&left, &right, sizeof(GUID)) < 0;
    }
};

typedef std::map<GUID, ModuleEntry*, GUIDLessComparer> Guid2Module;

typedef std::vector<ModuleEntry*> ModuleContainer;

struct ModuleLevel
{
    ModuleLevel() {}
    ModuleLevel(const wchar_t* c1, const wchar_t* c2)
        : cmt1(c1)
        , cmt2(c2)
    {}

    std::wstring cmt1;
    std::wstring cmt2;
};

typedef std::unordered_map<int, ModuleLevel> ModuleLevelMap;

}
