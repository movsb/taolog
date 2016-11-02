#include "stdafx.h"

#include "tooltip_window.h"

namespace taoetw {

void TooltipWindow::popup(const wchar_t* str, HFONT font)
{
    _text = str;
    _font = font;

    RECT rc {0,0,500,0};
    HDC hdc = ::GetDC(_hwnd);
    HFONT hOldFont = (HFONT)::SelectObject(hdc, _font);
    ::DrawText(hdc, _text, -1, &rc, DT_CALCRECT|DT_NOPREFIX);
    ::SelectObject(hdc, hOldFont);
    ::ReleaseDC(_hwnd, hdc);

    ::GetCursorPos(&_pt);
    ::SetWindowPos(_hwnd, HWND_TOPMOST, _pt.x + 10, _pt.y+10, rc.right - rc.left + 10, rc.bottom - rc.top + 10, SWP_NOACTIVATE);

    ::ShowWindow(_hwnd, SW_SHOWNOACTIVATE);

    ::SetTimer(_hwnd, 1, 250, 0);
}

void TooltipWindow::get_metas(WindowMeta* metas)
{
    __super::get_metas(metas);
    metas->style = WS_POPUP;
    metas->exstyle = WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED;
}

LRESULT TooltipWindow::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch(umsg)
    {
    case WM_CREATE:
    {
        ::SetLayeredWindowAttributes(_hwnd, 0, 245, LWA_ALPHA);

        break;
    }
    case WM_TIMER:
    {
        if(wparam == 1) {
            POINT pt;
            taowin::Rect rc;
            ::GetCursorPos(&pt);
            ::GetWindowRect(_hwnd, &rc);

            if((abs(pt.x - _pt.x) >= 5 || abs(pt.y - _pt.y) >= 5)
                && !::PtInRect(&rc, pt)
            )
            {
                ::KillTimer(_hwnd, 1);

                taowin::Rect rcClient;
                ::GetClientRect(_hwnd, &rcClient);
                HDC hdc = ::GetDC(_hwnd);
                ::FillRect(hdc, &rcClient, GetStockBrush(WHITE_BRUSH));
                ::ReleaseDC(_hwnd, hdc);
                ::ShowWindow(_hwnd, SW_HIDE);
            }
            return 0;
        }

        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(_hwnd, &ps);

        HFONT hOldFont = (HFONT)::SelectObject(hdc, _font);

        taowin::Rect rc;
        ::GetClientRect(_hwnd, &rc);

        HBRUSH hBrush = ::CreateSolidBrush(RGB(255, 255, 255));
        ::FillRect(hdc, &rc, hBrush);
        ::DeleteBrush(hBrush);

        ::SelectObject(hdc, GetStockBrush(NULL_BRUSH));
        Rectangle(hdc, 0, 0, rc.right, rc.bottom);

        ::SetBkMode(hdc, TRANSPARENT);

        rc.deflate(5, 5);
        ::DrawText(hdc, _text, -1, &rc, DT_NOPREFIX);

        ::SelectObject(hdc, hOldFont);

        ::EndPaint(_hwnd, &ps);

        break;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

}
