#include "main_window.h"
#include "config.h"

namespace taoetw {

static Config config;
Config& g_config = config;

}

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdline, int nShowCmd)
{
    taowin::init();

    taoetw::config.load(L"taoetw.json");

    taoetw::MainWindow main;

    main.create();
    main.show();

    taowin::loop_message();

    taoetw::config.save();

    return 0;
}
