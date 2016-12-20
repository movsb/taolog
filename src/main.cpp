#include "stdafx.h"

#include "misc/event_system.hpp"
#include "misc/config.h"
#include "misc/basic_async.h"

#include "gui/main_window.h"

namespace taoetw {

static Config config;
Config& g_config = config;

static EventSystem evtsys;
EventSystem& g_evtsys = evtsys;

static AsyncTaskManager async_manager;
AsyncTaskManager& g_async = async_manager;

}

namespace {
    void test()
    {

    }
}

#ifdef _DEBUG
int main() {
    setlocale(LC_ALL, "chs");
    DBG(L"Running...");
#else
int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdline, int nShowCmd) {
#endif 

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

    taoetw::config.save();

    return 0;
}
