#include "stdafx.h"

#include "utils.h"

namespace taoetw {

namespace utils {

void set_clipboard_text(const std::wstring& s)
{
    if (::OpenClipboard(nullptr)){
        int len = (s.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = ::GlobalAlloc(GHND, len);

        if (hMem){
            wchar_t* pmem = (wchar_t*)::GlobalLock(hMem);
            ::EmptyClipboard();
            ::memcpy(pmem, s.c_str(), len);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }

        CloseClipboard();

        if (hMem){
            GlobalFree(hMem);
        }
    }
}

}

}
