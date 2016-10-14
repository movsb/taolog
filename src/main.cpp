#include "main_window.h"

using namespace taoetw;

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdline, int nShowCmd)
{
    taowin::init();

    MainWindow main;

    main.create();
    main.show();

    taowin::loop_message();

    return 0;
}
