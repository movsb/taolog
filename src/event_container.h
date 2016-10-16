#pragma once

#include <string>
#include <vector>
#include <functional>
#include <regex>

#include "etwlogger.h"

namespace taoetw {

class EventContainer
{
protected:
    typedef ETWLogger::LogDataUI EVENT;
    typedef std::function<bool(const EVENT*)> FILTER;
    typedef std::vector<EVENT*> EVENTS;

public:
    EventContainer()
        : base_int(0)
        , base_value(0)
    {}

    EventContainer(const std::wstring& name, std::wstring& base, const std::wstring regex, int base_int, int base_value, std::wstring sbv)
        : name(name)
        , base(base)
        , rule(regex)
        , base_int(base_int)
        , base_value(base_value)
        , str_base_value(sbv)
    {
        _init();
    }

public:
    bool add(EVENT* evt);
    bool filter_results(EventContainer* container);
    EVENT* operator[](int index) { return _events[index]; }
    size_t size() const { return _events.size(); }

protected:
    void _init();

public:
    std::wstring name;
    std::wstring base;
    int          base_int;
    int          base_value;
    std::wstring str_base_value;
    std::wstring rule;

protected:

    EVENTS          _events;
    FILTER          _filter;

    std::wregex     _reobj;
};

typedef std::vector<EventContainer*> EventContainerS;

}
