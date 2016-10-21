#include "stdafx.h"

#include "_logdata_define.hpp"

#include "event_container.h"

namespace taoetw {

bool EventContainer::add(EVENT evt)
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
    _reobj = std::wregex(rule, std::regex_constants::icase);
    _filter = [&](const EVENT evt) {
        // 这里我不知道怎么根据base_int拿字段，所以特殊处理（为了效率）
        const wchar_t* p = nullptr;

        bool processed = true;

        switch (base_int)
        {
        case 0: p = evt->id;                    break;
        case 1: p = evt->strTime.c_str();       break;
        case 2: p = evt->strPid.c_str();        break;
        case 3: p = evt->strTid.c_str();        break;
        case 4: p = evt->strProject.c_str();    break;
        case 5: p = evt->file;                  break;
        case 6: p = evt->func;                  break;
        case 7: p = evt->strLine.c_str();       break;
        case 9: p = evt->text;                  break;
        default: processed = false;             break;
        }

        if(processed) return !p || !std::regex_search(p, _reobj);
        else {
            switch(base_int)
            {
            case 8: return evt->level != base_value;
            }
        }

        assert(0);
        return false;
    };
}

}

