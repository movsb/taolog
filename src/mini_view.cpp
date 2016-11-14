#include "stdafx.h"

#include "config.h"

#include "_logdata_define.hpp"
#include "_module_entry.hpp"

#include "event_container.h"
#include "listview_color.h"

#include "mini_view.h"

namespace taoetw {

RECT MiniView::winpos = {-1,-1};

// TODO 这段代码也是直接从 main 中搬过来的
void MiniView::update()
{
    // 默认是非自动滚屏到最后一行的
    // 但如果当前焦点行是最后一行，则自动滚屏
    int count = (int)_events.size();
    int sic_flag = LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL;
    bool is_last_focused = count > 1 && (_listview->get_item_state(count - 2, LVIS_FOCUSED) & LVIS_FOCUSED)
        || _listview->get_next_item(-1, LVIS_FOCUSED) == -1;

    if(is_last_focused) {
        sic_flag &= ~LVSICF_NOSCROLL;
    }

    _listview->set_item_count(count, sic_flag);

    if(is_last_focused) {
        _listview->set_item_state(-1, LVIS_FOCUSED | LVIS_SELECTED, 0);
        _listview->ensure_visible(count - 1);
        _listview->set_item_state(count - 1, LVIS_FOCUSED|LVIS_SELECTED, LVIS_FOCUSED|LVIS_SELECTED);
    }
}

LPCTSTR MiniView::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="精简视图" size="300,300">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
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

        // 默认置顶
        ::SetWindowPos(_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // 恢复窗口位置
        if(winpos.left != -1 && winpos.top != -1) {
            taowin::Rect rc = winpos;
            ::SetWindowPos(_hwnd, nullptr, rc.left, rc.top, rc.width(), rc.height(), SWP_NOZORDER);
        }

        _listview->show_header(0);
        _listview->insert_column(L"内容", 0, 0);

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
        ::GetWindowRect(_hwnd, &winpos);

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
            auto field = (*evt)[9]; // TODO 硬编码

            lit->pszText = const_cast<LPWSTR>(field);

            return 0;
        }
        else if(code == NM_CUSTOMDRAW) {
            return _on_custom_draw_listview(hdr);
        }
    }

    return __super::on_notify(hwnd, pc, code, hdr);
}

LRESULT MiniView::_on_custom_draw_listview(NMHDR* hdr)
{
    LRESULT lr = CDRF_DODEFAULT;

    auto lvcd = (LPNMLVCUSTOMDRAW)hdr;

    switch (lvcd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        lr = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEM | CDDS_PREPAINT:
        auto& log = *_events[lvcd->nmcd.dwItemSpec];
        lvcd->clrText = _colors[log.level].fg;
        lvcd->clrTextBk = _colors[log.level].bg;
        lr = CDRF_NEWFONT;
        break;
    }

    return lr;
}


}
