/*
 * http://unixwiz.net/techtips/outputdebugstring.html
 */


#include <string>

#include <Windows.h>
#include <process.h>

#include "dbgview.h"

namespace taolog
{

bool DebugView::init(OnNotify notify)
{
    if(_hThread) return false;

    _notify = notify;

    HANDLE hTest = ::OpenEvent(SYNCHRONIZE, FALSE, L"DBWIN_BUFFER_READY");
    if(hTest) {
        ::CloseHandle(hTest);
        return false;
    }

    _hEvtBufReady = ::CreateEvent(nullptr, FALSE, TRUE, L"DBWIN_BUFFER_READY");
    _hEvtDataReady = ::CreateEvent(nullptr, FALSE, FALSE, L"DBWIN_DATA_READY");
    _hEvtExit = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

    _hFile = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, buffer_size, L"DBWIN_BUFFER");
    if(_hFile) _pLog = (DbWinBuffer*)::MapViewOfFile(_hFile, FILE_MAP_READ, 0, 0, buffer_size);

    if(!_hEvtBufReady || !_hEvtDataReady || !_hEvtExit || !_hFile || !_pLog) {
        uninit();
        return false;
    }

    _hThread = (HANDLE)::_beginthreadex(nullptr, 0, __ThreadProc, this, 0, nullptr);
    if(!_hThread) {
        uninit();
        return false;
    }

    return true;
}

void DebugView::uninit()
{
    auto close = [](HANDLE& h) {
        if(h != nullptr && h != INVALID_HANDLE_VALUE) {
            ::CloseHandle(h);
            h = nullptr;
        }
    };

    if(_hThread) {
        ::SetEvent(_hEvtExit);
        ::WaitForSingleObject(_hThread, INFINITE);
        close(_hThread);
    }

    if(_pLog) {
        ::UnmapViewOfFile(_pLog);
        _pLog = nullptr;
    }

    close(_hFile);

    close(_hMutex);
    close(_hEvtExit);
    close(_hEvtBufReady);
    close(_hEvtDataReady);
}

unsigned int DebugView::ThreadProc()
{
    HANDLE handles[2] = {_hEvtExit, _hEvtDataReady};

    for(bool loop = true; loop;) {
        switch(::WaitForMultipleObjects(_countof(handles), handles, FALSE, INFINITE))
        {
        case WAIT_OBJECT_0 + 0:
            ::ResetEvent(_hEvtExit);
            //::WaitForSingleObject(_hEvtDataReady, INFINITE);
            loop = false;
            break;

        case WAIT_OBJECT_0 + 1:
            if(_notify)
                _notify(_pLog->pid, _pLog->str);

            ::SetEvent(_hEvtBufReady);
            loop = true;
            break;

        default:
            loop = false;
            break;
        }
    }

    return 0;
}

unsigned int DebugView::__ThreadProc(void * ud)
{
    auto dv = static_cast<DebugView*>(ud);
    return dv->ThreadProc();
}

}
