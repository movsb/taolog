#include "stdafx.h"

#include "tooltip_window.h"

namespace taoetw {

void TooltipWindow::popup(const wchar_t* str, HFONT font)
{
    _text = str;
    _font = font;

    taowin::Rect rc {0,0,::GetSystemMetrics(SM_CXSCREEN) - padding * 2,0};
    HDC hdc = ::GetDC(_hwnd);
    HFONT hOldFont = (HFONT)::SelectObject(hdc, _font);
    ::DrawText(hdc, _text, -1, &rc, DT_CALCRECT|DT_NOPREFIX|DT_WORDBREAK|DT_EDITCONTROL);
    ::SelectObject(hdc, hOldFont);
    ::ReleaseDC(_hwnd, hdc);

    adjust_window_pos(rc);

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

            if((abs(pt.x - _pt.x) > offset || abs(pt.y - _pt.y) > offset)
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

        rc.deflate(padding, padding);
        ::DrawText(hdc, _text, -1, &rc, DT_NOPREFIX|DT_WORDBREAK|DT_EDITCONTROL);

        ::SelectObject(hdc, hOldFont);

        ::EndPaint(_hwnd, &ps);

        break;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

// 调整窗口的最终位置
void TooltipWindow::adjust_window_pos(const taowin::Rect& calc)
{
    // calc 是计算出的文字所占据的大小
    taowin::Rect rc(calc);

    // 加上文字外边距
    rc.inflate(padding, padding);

    // 屏幕信息
    taowin::Rect rcScreen;
    {
        HMONITOR hMonitor = ::MonitorFromPoint(_pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info = {sizeof(info)};

        if(hMonitor && ::GetMonitorInfo(hMonitor, &info)) {
            rcScreen = info.rcWork;
        }
        else {
            rcScreen = {0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN)};
        }
    }

    rcScreen.deflate(padding, padding);

    // 限制大小为屏幕大小（工作区）
    if(rc.width() > rcScreen.width())   rc.right = rc.left + rcScreen.width();
    if(rc.height() > rcScreen.height()) rc.bottom = rc.top + rcScreen.height();

    // 根据当前鼠标定位原点
    ::GetCursorPos(&_pt);
    rc.offset(_pt.x + offset, _pt.y + offset);

    // 不能超出屏幕边缘
    int dxscr = rc.right - rcScreen.right;
    int dyscr = rc.bottom - rcScreen.bottom;
    if(dxscr > 0 || dyscr > 0) {
        int dx = std::max(dxscr, 0);
        int dy = std::max(dyscr, 0);
        rc.offset(-dx, -dy);
    }

    ::SetWindowPos(_hwnd, HWND_TOPMOST, rc.left, rc.top, rc.width(), rc.height(), SWP_NOACTIVATE);
}

}
