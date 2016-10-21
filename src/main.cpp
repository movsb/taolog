#include "stdafx.h"

#include "config.h"

#include "main_window.h"

namespace taoetw {

static Config config;
Config& g_config = config;

}

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdline, int nShowCmd)
{
    taowin::init();

    taoetw::config.load(L"taoetw.json");

    auto main = new taoetw::MainWindow;

    main->create();
    main->show();

    taowin::loop_message();

    taoetw::config.save();

    return 0;
}
