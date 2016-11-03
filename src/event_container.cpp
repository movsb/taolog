#include "stdafx.h"

#include "_logdata_define.hpp"

#include "event_container.h"

namespace taoetw {

bool EventContainer::add(EVENT& evt)
{
    if (!_filter || !_filter(evt)) {
        _events.push_back(evt);
        return true;
    }
    else {
        return false;
    }
}

bool EventContainer::filter_results(EventContainer* container)
{
    assert(container != this);

    for (auto& evt : _events)
        container->add(evt);

    return !container->_events.empty();
}

void EventContainer::_init()
{
    _filter = [&](const EVENT& evt) {
        const auto p = (*evt)[field_index];

        switch(field_index)
        {
        // 编号，时间，进程，线程，行号
        // 直接执行相等性比较（区分大小写）
        case 0: case 1: case 2: case 3: case 7:
        {
            return p != value_input;
        }
        // 文件，函数，日志
        // 执行不区分大小写的搜索
        case 5: case 6: case 9:
        {
            auto tolower = [](std::wstring& s) {
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                return s;
            };

            auto haystack = tolower(std::wstring(p));
            auto needle = tolower(value_input);

            return !::wcsstr(haystack.c_str(), needle.c_str());
        }
        // 项目
        case 4:
        {
            return value_name != p;
        }
        // 等级
        case 8:
        {
            return value_index != evt->level;
        }
        default:
            assert(0 && L"invalid index");
            return true;
        }
    };
}

}
