#include "stdafx.h"

#include <new>

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

    std::set_new_handler([] {
        ::MessageBox(nullptr, L"内存不足。", nullptr, MB_ICONEXCLAMATION);
    });

    test();

    taolog::config.load(L"taolog.json");

    int what = taolog::g_config->obj("basic")["type"].int_value();
    if(what == 0) {
        what = ::MessageBox(nullptr, L"【是】启动 EtwLog；\n【否】启动 DebugView。", L"", MB_ICONQUESTION | MB_YESNO);
        what = what == IDYES ? taolog::LogSysType::EventTracing : taolog::LogSysType::DebugView;
    }

    taowin::init();
    taowin::window_creator* main;
    main = new taolog::MainWindow((taolog::LogSysType::Value)what);
    main->create();
    main->show();

    taowin::loop_message();

    taolog::config.save();

    return 0;
}
