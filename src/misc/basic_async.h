#pragma once

#include <Windows.h>

#include <vector>
#include <list>
#include <functional>

namespace taoetw
{

class AsyncTask
{
    friend class AsyncTaskManager;

public:
    AsyncTask(void* ud = nullptr)
        : _ud(ud)
        , _ret(0)
    { }

    int get_ret() const { return _ret; }

protected:
    virtual int  doit() = 0;
    virtual int  done() = 0;
    virtual void update(double ps) {}

protected:
    void set_ps(double ps);

protected:
    int     _ret;
    void*   _ud;

private:
    void set_mgr(AsyncTaskManager* mgr) { _mgr = mgr; }
    void set_ret(int ret) { _ret = ret; }

private:
    AsyncTaskManager* _mgr;
    double            _ps;
};

class AsyncTaskManager
{
protected:
    struct MGRMSG
    {
        enum Value
        {
            __Start = WM_USER,
            MarkBusy,
            MarkIdle,
            GetTask,
            DoneTask,
            UpdateProgress,
        };
    };

public:
    AsyncTaskManager()
        : _hwnd(nullptr)
        , _nIdleThread(0)
    {
        RegisterWindowClass();
        CreateWnd();
        _hEvtGetTask = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    ~AsyncTaskManager()
    {
        DestroyWnd();
        ::CloseHandle(_hEvtGetTask);
        _hEvtGetTask = nullptr;
    }

    void AddTask(AsyncTask* pTask);
    void UpdateProgress(AsyncTask* pTask);

protected:
    LRESULT SendMsg(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0);
    LRESULT ProcessMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    unsigned int ThreadProc();
    void CreateWnd();
    void DestroyWnd();
    void MarkThreadBusy();
    void MarkThreadIdle();
    AsyncTask* WaitTask();
    void DoneTask(AsyncTask* pTask, int ret);

private:
    static void RegisterWindowClass();
    static LRESULT CALLBACK __WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static unsigned int __stdcall __ThreadProc(void* ud);

private:
    HWND                    _hwnd;
    int                     _nIdleThread;
    std::vector<HANDLE>     _threads;
    std::list<AsyncTask*>   _tasks;
    HANDLE                  _hEvtGetTask;
};

extern AsyncTaskManager& g_async;

}
