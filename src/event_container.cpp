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
    _reobj = std::wregex(rule, std::regex_constants::icase);
    _filter = [&](const EVENT& evt) {
        // 这里我不知道怎么根据base_int拿字段，所以特殊处理（为了效率）
        const wchar_t* p = nullptr;

        bool processed = true;

        switch (base_int)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 9:
            p = (*evt)[base_int];
            break;

        default:
            processed = false;
            break;
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

