#include <iostream>

#include <windows.h>
#include <guiddef.h>

#define ETW_LOGGER

#include "etwlogger.h"

// {630514B5-7B96-4B74-9DB6-66BD621F9386}
static const GUID providerGuid = 
{ 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } };

ETWLogger g_etwLogger(providerGuid);


int main()
{
start:

    EtwVbs(_T("Verbose\n123333333333333333333333333333333\n555555555555555\nafasdfasfasfd\n\n4028402394"));
    EtwLog(_T("Information"));
    EtwWrn(_T("Warning"));
    EtwErr(_T("Error"));
    EtwFat(_T("Critical"));
    EtwVbs(_T("Long string jasl;alkdjfklasfjdklajsfeaskdlfjsakdfjasklfj asalalfas asdfaslfk"));

    Sleep(3000);

    goto start;

    return 0;
}
