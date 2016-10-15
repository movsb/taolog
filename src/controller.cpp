#include "controller.h"

namespace taoetw {

bool Controller::start(const wchar_t* session)
{
    _name = session;

    size_t size = sizeof(EVENT_TRACE_PROPERTIES) + (_name.size() + 1)*sizeof(TCHAR);

    // TODO 这里写得很有问题
restart:
    _props = (EVENT_TRACE_PROPERTIES*)new char[size]();
    _props->Wnode.BufferSize = size;
    _props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    _props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    _props->LogFileNameOffset = 0;

    _props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    _props->LogFileNameOffset = 0;
    _props->FlushTimer = 1;

    wcscpy((wchar_t*)_props + _props->LoggerNameOffset, session);

    auto ret = ::StartTrace(&_handle, _name.c_str(), _props);
    
    if(ret == ERROR_ALREADY_EXISTS) {
        ::ControlTrace(0, _name.c_str(), _props, EVENT_TRACE_CONTROL_STOP);
        goto restart;
    }

    return ret == ERROR_SUCCESS && _handle != 0;
}

bool Controller::enable(const GUID& provider, bool enable_, int level)
{
    return ::EnableTrace(enable_, 0, TRACE_LEVEL_INFORMATION, &provider, _handle) == ERROR_SUCCESS;
}

void Controller::stop()
{
    if (started()) {
        ::ControlTrace(0, _name.c_str(), _props, EVENT_TRACE_CONTROL_STOP);
        _handle = 0;
    }
    // delete[] _props;
}

bool Controller::started()
{
    return _handle && _handle != INVALID_PROCESSTRACE_HANDLE;
}

} // namespace taoetw
