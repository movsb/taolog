#pragma once

#include <string>
#include <windows.h>

namespace taoetw {
    std::wstring last_error()
    {
        std::wstring r;
        wchar_t* b = L"";

        if (::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPTSTR)&b, 1, NULL))
        {
            r = b;
            ::LocalFree(b);
        }

        return std::move(r);
    }
}
