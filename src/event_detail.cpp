#include "event_detail.h"

namespace taoetw {

LPCTSTR EventDetail::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="ÏêÇé" size="512,480">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <edit name="text" font="consolas" style="multiline,vscroll,hscroll,readonly" exstyle="clientedge"/>
        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT EventDetail::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        auto edit = _root->find<taowin::edit>(L"text");
        _hText = edit->hwnd();
        edit->set_text(_log->text);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wparam;
        HWND hwnd = (HWND)lparam;

        if (hwnd == _hText) {
            ::SetTextColor(hdc, _fg);
            ::SetBkColor(hdc, _bg);
            return LRESULT(_hbrBk);
        }

        break;
    }

    }
    return __super::handle_message(umsg, wparam, lparam);
}

}
