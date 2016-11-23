#include "stdafx.h"

#include "misc/event_system.hpp"
#include "misc/config.h"

#include "gui/main_window.h"

namespace taoetw {

static Config config;
Config& g_config = config;

static EventSystem evtsys;
EventSystem& g_evtsys = evtsys;

}

namespace {
    void test()
    {

    }
}

namespace taoetw {
    extern void DoEtwLog(LogDataUI* log);
    extern LogDataUI* DoEtwAlloc();
}

static LRESULT CALLBACK __WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_COPYDATA) {
        auto cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        auto log = reinterpret_cast<taoetw::LogData*>(cds->lpData);
        taoetw::DoEtwLog(log);
        return 0;
    }

    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void RegisterLoggerWindowClass()
{
    static bool bRegistered = false;

    if(bRegistered) return;

    WNDCLASSEX wcx = {sizeof(wcx)};
    wcx.lpszClassName = _T("{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}");
    wcx.lpfnWndProc = __WindowProcedure;
    wcx.hInstance = ::GetModuleHandle(NULL);
    wcx.cbWndExtra = sizeof(void*);

    bRegistered = !!::RegisterClassEx(&wcx);
}

#ifdef _DEBUG
int main() {
    setlocale(LC_ALL, "chs");
    DBG(L"Running...");
#else
int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdline, int nShowCmd) {
#endif 
    RegisterLoggerWindowClass();

    HWND hHostWnd = ::CreateWindowEx(0, 
        L"{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}",
        L"{6E5E5CBC-8ACF-4fa6-98E4-0C63A075323B}::HostWnd",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);

    test();

    taoetw::config.load(L"taoetw.json");

    int what = ::MessageBox(nullptr, L"¡¾ÊÇ¡¿Æô¶¯ EtwLog£»\n¡¾·ñ¡¿Æô¶¯ DebugView¡£", L"", MB_ICONQUESTION | MB_YESNOCANCEL);
    if(what != IDCANCEL) {
        taowin::init();

        taowin::window_creator* main;

        main = new taoetw::MainWindow(what == IDYES ? taoetw::LogSysType::EventTracing : taoetw::LogSysType::DebugView);

        main->create();
        main->show();

        taowin::loop_message();
    }

    ::DestroyWindow(hHostWnd);

    taoetw::config.save();

    return 0;
}
