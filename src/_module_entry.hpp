#pragma once

#include <map>
#include <unordered_map>
#include <string>
#include <guiddef.h>
#include <vector>

namespace taoetw {

struct ModuleEntry
{
    std::wstring    name;
    std::wstring    root;
    bool            enable;
    unsigned char   level;
    GUID            guid;
    std::wstring    guid_str;
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
