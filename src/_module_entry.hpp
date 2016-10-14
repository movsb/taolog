#pragma once

#include <map>
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
};

struct GUIDLessComparer {
    bool operator()(const GUID& left, const GUID& right) const
    {
        return ::memcmp(&left, &right, sizeof(GUID)) < 0;
    }
};

typedef std::map<GUID, ModuleEntry*, GUIDLessComparer> Guid2Module;

typedef std::vector<ModuleEntry*> ModuleContainer;

}
