#include "stdafx.h"

#include "misc/config.h"
#include "_logdata_define.hpp"

#include "event_container.h"

namespace taoetw {

json11::Json EventContainer::to_json() const
{
    return json11::Json::object {
        {"name",        g_config.us(name)       },
        {"field_index", field_index             },
        {"field_name",  g_config.us(field_name) },
        {"value_index", value_index             },
        {"value_name",  g_config.us(value_name) },
        {"value_input", g_config.us(value_input)},
    };
}

std::wstring EventContainer::to_tip() const
{
    std::wstring str;

    str.reserve(2048);

    str += L"名字：" + name + L"\bn";
    str += L"字段：" + field_name + L"\bn";
    str += L"文本：" + (!value_input.empty() ? value_input : value_name) + L"\bn";
    str += L"临时：" + std::wstring(is_tmp ? L"是" : L"否") + L"\bn";

    return std::move(str);
}

EventContainer* EventContainer::from_json(const json11::Json& obj)
{
    EventContainer* p = nullptr;

    auto name           = obj["name"];
    auto field_index    = obj["field_index"];
    auto field_name     = obj["field_name"];
    auto value_index    = obj["value_index"];
    auto value_name     = obj["value_name"];
    auto value_input    = obj["value_input"];

    if(name.is_string()
        && field_index.is_number()
        && field_name.is_string()
        && value_index.is_number()
        && value_name.is_string()
        && value_input.is_string()
        )
    {
        p = new EventContainer(
            g_config.ws(name.string_value()),
            field_index.int_value(),
            g_config.ws(field_name.string_value()),
            value_index.int_value(),
            g_config.ws(value_name.string_value()),
            g_config.ws(value_input.string_value())
        );
    }

    return p;
}

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

void EventContainer::enable_filter(bool b)
{
    if(!b) {
        _filter = nullptr;
        return;
    }
    else {
        auto tolower = [](std::wstring s) {
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        };

        _value_input_lower = tolower(value_input);

        _filter = [&](const EVENT& evt) {
            const auto p = (*evt)[field_index];

            auto search_value_in_p = [&] {

                auto haystack = tolower(std::wstring(p));
                auto& needle = _value_input_lower;

                return !::wcsstr(haystack.c_str(), needle.c_str());
            };

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
                return search_value_in_p();
            }
            // 模块
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

}

