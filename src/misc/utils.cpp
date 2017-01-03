#include "stdafx.h"

#include "utils.h"

namespace taolog {

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

std::wstring get_clipboard_text()
{
    std::wstring s;

    if(::OpenClipboard(nullptr)) {
        if(HANDLE clip = ::GetClipboardData(CF_UNICODETEXT)) {
            if(auto p = reinterpret_cast<wchar_t*>(::GlobalLock(clip))) {
                s = p;
                ::GlobalUnlock(p);
            }
        }
        ::CloseClipboard();
    }

    return std::move(s);
}

}

}
