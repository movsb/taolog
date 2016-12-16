#include "basic_async.h"

#include <assert.h>
#include <process.h>

namespace taoetw
{

void AsyncTaskManager::AddTask(AsyncTask* pTask)
{
    if(_nIdleThread <= 0) {
        HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, __ThreadProc, this, 0, nullptr);
        assert(hThread);
        _threads.emplace_back(hThread);
    }

    _tasks.emplace_back(pTask);
    ::SetEvent(_hEvtGetTask);
}

unsigned int AsyncTaskManager::ThreadProc()
{
    for(AsyncTask* pTask; pTask = WaitTask();)
    {
        auto ret = pTask->doit();
        DoneTask(pTask, ret);
    }

    return 0;
}

void AsyncTaskManager::CreateWnd()
{
    (void)::CreateWindowEx(0, L"{A9F6B4D1-357D-4A5D-8220-BA6C5228054A}", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, this);
}

void AsyncTaskManager::DestroyWnd()
{
    ::DestroyWindow(_hwnd);
    _hwnd = nullptr;
}

void AsyncTaskManager::MarkThreadBusy()
{
    SendMsg(MGRMSG::MarkBusy);
}

void AsyncTaskManager::MarkThreadIdle()
{
    SendMsg(MGRMSG::MarkIdle);
}

AsyncTask* AsyncTaskManager::WaitTask()
{
    MarkThreadIdle();
    ::WaitForSingleObject(_hEvtGetTask, INFINITE);
    AsyncTask* pTask = (AsyncTask*)SendMsg(MGRMSG::GetTask);
    if(pTask) MarkThreadBusy();
    return pTask;
}

void AsyncTaskManager::DoneTask(AsyncTask* pTask, int ret = 0)
{
    SendMsg(MGRMSG::DoneTask, ret, LPARAM(pTask));
}

LRESULT AsyncTaskManager::SendMsg(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return ::SendMessage(_hwnd, uMsg, wParam, lParam);
}

LRESULT AsyncTaskManager::ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
    case MGRMSG::MarkIdle:
    {
        _nIdleThread++;
        return 0;
    }
    case MGRMSG::MarkBusy:
    {
        _nIdleThread--;
        return 0;
    }
    case MGRMSG::GetTask:
    {
        AsyncTask* pTask = nullptr;

        if(!_tasks.empty()) {
            pTask = _tasks.front();
            _tasks.pop_front();
        }

        return LRESULT(pTask);
    }
    case MGRMSG::DoneTask:
    {
        auto pTask = reinterpret_cast<AsyncTask*>(lParam);
        int ret = static_cast<int>(wParam);
        pTask->set_ret(ret);
        pTask->done();
        return 0;
    }
    }

    return ::DefWindowProc(_hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK AsyncTaskManager::__WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto mgr = reinterpret_cast<AsyncTaskManager*>(::GetWindowLongPtr(hWnd, 0));

    if(uMsg == WM_NCCREATE) {
        auto p = reinterpret_cast<LPCREATESTRUCT>(lParam);
        mgr = static_cast<AsyncTaskManager*>(p->lpCreateParams);
        mgr->_hwnd = hWnd;
        ::SetWindowLongPtr(hWnd, 0, LONG(mgr));
    }

    return mgr
        ? mgr->ProcessMessage(uMsg, wParam, lParam)
        : ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void AsyncTaskManager::RegisterWindowClass()
{
    static bool bRegistered = false;

    if(bRegistered) return;

    WNDCLASSEX wcx = {sizeof(wcx)};

    wcx.lpszClassName = L"{A9F6B4D1-357D-4A5D-8220-BA6C5228054A}";
    wcx.lpfnWndProc = __WindowProcedure;
    wcx.hInstance = ::GetModuleHandle(NULL);
    wcx.cbWndExtra = sizeof(void*);

    bRegistered = !!::RegisterClassEx(&wcx);
}

unsigned int __stdcall AsyncTaskManager::__ThreadProc(void* ud)
{
    auto mgr = reinterpret_cast<AsyncTaskManager*>(ud);
    return mgr->ThreadProc();
}

}
