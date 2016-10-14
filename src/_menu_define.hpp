#pragma once

#include <string>
#include <vector>
#include <functional>

namespace taoetw {

struct MenuEntry
{
    std::wstring name;
    std::function<void()> onclick;
};

typedef std::vector<MenuEntry> MenuContainer;

}
