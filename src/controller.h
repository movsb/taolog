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
    void start(const wchar_t* session);
    void enable(const GUID& provider, bool enable_, const char* level);
    void stop();

protected:
    TRACEHANDLE             _handle;
    EVENT_TRACE_PROPERTIES* _props;
    std::wstring            _name;
};

}
