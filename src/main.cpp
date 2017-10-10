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
        ::MessageBox(nullptr, L"ÄÚ´æ²»×ã¡£", nullptr, MB_ICONEXCLAMATION);
    });

    test();

    taolog::config.load(L"taolog.json");

    taowin::init();

    taowin::WindowCreator* main;

    main = new taolog::MainWindow(::GetAsyncKeyState(VK_CONTROL) & 0x8000 ? taolog::LogSysType::DebugView : taolog::LogSysType::EventTracing);

    main->create();
    main->show();

    taowin::loop_message();

    taolog::config.save();

    return 0;
}
