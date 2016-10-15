#pragma once

#include <string>

#include <Windows.h>
#include <evntrace.h>

namespace taoetw {

class Controller
{
public:
    Controller()
        : _handle(0)
        , _props(nullptr)
    { }

    ~Controller()
    {
        stop();
    }

public:
    bool start(const wchar_t* session);
    bool enable(const GUID& provider, bool enable_, int level);
    void stop();
    bool started();

protected:
    TRACEHANDLE             _handle;
    EVENT_TRACE_PROPERTIES* _props;
    std::wstring            _name;
};

}
