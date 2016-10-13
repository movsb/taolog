#include <iostream>

#include <guiddef.h>

#define ETW_LOGGER

#include "etwlogger.h"

// {630514B5-7B96-4B74-9DB6-66BD621F9386}
static const GUID providerGuid = 
{ 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } };

ETWLogger g_etwLogger(providerGuid);


int main()
{
    ETW_LEVEL_INFORMATION(L"Information");
    ETW_LEVEL_WARNING(L"Warning");
    ETW_LEVEL_ERROR(L"Error");
    ETW_LEVEL_CRITICAL(L"Critical");
    ETW_LEVEL_VERBOSE(L"Verbose");

    return 0;
}
