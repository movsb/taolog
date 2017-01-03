#include "stdafx.h"

#include "misc/event_system.hpp"
#include "misc/config.h"
#include "misc/basic_async.h"

#include "gui/main_window.h"

namespace taolog {

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

#ifdef THUNDER_RELEASE
    taolog::config.load(L"logview.json");
#else
    taolog::config.load(L"taolog.json");
#endif

    int what = ::MessageBox(nullptr, L"¡¾ÊÇ¡¿Æô¶¯ EtwLog£»\n¡¾·ñ¡¿Æô¶¯ DebugView¡£", L"", MB_ICONQUESTION | MB_YESNOCANCEL);
    if(what != IDCANCEL) {
        taowin::init();

        taowin::window_creator* main;

        main = new taolog::MainWindow(what == IDYES ? taolog::LogSysType::EventTracing : taolog::LogSysType::DebugView);

        main->create();
        main->show();

        taowin::loop_message();
    }

    taolog::config.save();

    return 0;
}
