#include "stdafx.h"

#include "_logdata_define.hpp"
#include "event_container.h"

#include "mini_view.h"

namespace taoetw {

void MiniView::update()
{
    _listview->set_item_count(_events.size(), LVSICF_NOINVALIDATEALL);
}

LPCTSTR MiniView::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="¾«¼òÊÓÍ¼" size="300,300">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
    </res>
    <root>
        <vertical padding="3,3,3,3">
            <listview name="lv" style="showselalways,ownerdata" exstyle="clientedge,doublebuffer"/>
        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT MiniView::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        _listview = _root->find<taowin::listview>(L"lv");

        // Ä¬ÈÏÖÃ¶¥
        ::SetWindowPos(_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // ÓÐ BUG °¡£¬¾¹È»²»×Ô¶¯ÖÃ¶¥
        HWND hTooltip = ListView_GetToolTips(_listview->hwnd());
        ::SetWindowPos(hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

        _listview->show_header(0);
        _listview->insert_column(L"ÄÚÈÝ", 0, 0);

        return 0;
    }
    case WM_SIZE:
    {
        RECT rc;
        ::GetClientRect(_listview->hwnd(), &rc);
        _listview->set_column_width(0, rc.right - rc.left - 5);
        break;
    }
    case WM_CLOSE:
    {
        _on_close();
        DestroyWindow(_hwnd);
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT MiniView::on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr)
{
    if(pc == _listview) {
        if(code == LVN_GETDISPINFO) {
            auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
            auto& evt = _events[pdi->item.iItem];
            auto lit = &pdi->item;
            auto field = (*evt)[9]; // TODO Ó²±àÂë

            lit->pszText = const_cast<LPWSTR>(field);

            return 0;
        }
    }

    return __super::on_notify(hwnd, pc, code, hdr);
}

}
