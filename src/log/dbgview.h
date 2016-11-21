#pragma once

#include <functional>

#include <Windows.h>

namespace taoetw {

class DebugView
{
    static constexpr int buffer_size = 4096; // fixed, don't change

    struct DbWinBuffer
    {
        DWORD pid;
        char str[buffer_size - sizeof(DWORD)];
    };

    typedef std::function<void(DWORD pid, const char* str)> OnNotify;

public:
    DebugView()
        : _hThread(nullptr)
        , _hMutex(nullptr)
        , _hFile(nullptr)
        , _hEvtBufReady(nullptr)
        , _hEvtDataReady(nullptr)
        , _pLog(nullptr)
    { }

public:
    bool init(OnNotify notify); // notify 调用于非 init 时的线程
    void uninit();

protected:
    static unsigned int __stdcall __ThreadProc(void* ud);
    unsigned int ThreadProc();

protected:
    OnNotify        _notify;

protected:
    HANDLE          _hThread;       // 接收线程
    HANDLE          _hMutex;        // OutputDebugString 互斥体
    HANDLE          _hFile;         // 共享内存
    HANDLE          _hEvtBufReady;  // 共享内存闲置
    HANDLE          _hEvtDataReady; // 数据备妥
    HANDLE          _hEvtExit;      // 退出事件
    DbWinBuffer*    _pLog;          // 共享内存映射
};

}
