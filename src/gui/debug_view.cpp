#include "stdafx.h"

#include "misc/config.h"
#include "misc/mem_pool.hpp"

#include "_logdata_define.hpp"
#include "_module_entry.hpp"

#include "event_container.h"
#include "listview_color.h"
#include "column_selection.h"
#include "result_filter.h"
#include "tooltip_window.h"

#include "log/dbgview.h"

#include "debug_view.h"

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

void MiniView::_clear_results()
{
    for (auto& f : _filters)
        f->clear();

    _events.clear();

    _listview->set_item_count(0, 0);
}

LRESULT MiniView::_on_logcmd(LogMessage::Value cmd, LPARAM lParam)
{
    if(cmd == LogMessage::Alloc) {
        return (LRESULT)_logpool.alloc();
    }
    else if(cmd == LogMessage::Dealloc) {
        auto p = reinterpret_cast<LogDataUI*>(lParam);
        p->~LogDataUI();
        _logpool.destroy(p);
        return 0;
    }
    else if(cmd == LogMessage::Log) {
        LogDataUIPtr item((LogDataUI*)lParam, [&](LogDataUI* p) {
            _on_logcmd(LogMessage::Dealloc, LPARAM(p));
        });

        // 日志编号
        _snwprintf(item->id, _countof(item->id), L"%llu", (unsigned long long)_events.size() + 1);

        item->offset_of_file = 0;

        // 字符串形式的日志等级
        item->strLevel = nullptr;

        // 全部事件容器
        _events.add(item);

        // 判断一下当前过滤器是否添加了此事件
        // 如果没有添加，就不必要刷新列表控件了
        bool added_to_current = _curflt == &_events;

        // 带过滤的事件容器（指针复用）
        if(!_filters.empty()) {
            for(auto& f : _filters) {
                bool added = f->add(item);
                if(f == _curflt && added && !added_to_current)
                    added_to_current = true;
            }
        }

        if(added_to_current) {
            // 默认是非自动滚屏到最后一行的
            // 但如果当前焦点行是最后一行，则自动滚屏
            int count = (int)_curflt->size();
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
                _listview->set_item_state(count - 1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            }
        }

        return 0;
    }

    return 0;
}

LPCTSTR MiniView::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="DebugView" size="600,400">
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
    case kLogCmd:
        return _on_logcmd(LogMessage::Value(wparam), lparam);

    case WM_CREATE:
    {
        _curflt = &_events;

        _listview = _root->find<taowin::listview>(L"lv");

        // 默认置顶
        ::SetWindowPos(_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // 恢复窗口位置
        if(winpos.left != -1 && winpos.top != -1) {
            taowin::Rect rc = winpos;
            ::SetWindowPos(_hwnd, nullptr, rc.left, rc.top, rc.width(), rc.height(), SWP_NOZORDER);
        }

        _columns.emplace_back(L"编号", true,   50, "id"   ,0);
        _columns.emplace_back(L"时间", true,   86, "time" ,1);
        _columns.emplace_back(L"进程", true ,  50, "pid"  ,2);
        _columns.emplace_back(L"日志", true,  300, "log"  ,3);

        for (int i = 0; i < (int)_columns.size(); i++) {
            auto& col = _columns[i];
            _listview->insert_column(col.name.c_str(), col.show ? col.width : 0, i);
        }

        subclass_control(_listview);

        async_call([&] {
            auto fnGetFields = [&](std::vector<std::wstring>* fields, int* def) {
                for(auto& c : _columns) {
                    fields->emplace_back(c.name);
                }

                *def = (int)_columns.size() - 1;
            };

            auto fnGetValues = [&](int idx, std::unordered_map<int, const wchar_t*>* values) {
                values->clear();
            };

            AddNewFilter dlg(fnGetFields, fnGetValues);
            if(dlg.domodal(this) == IDOK) {
                _events.name        = dlg.name;
                _events.field_index = dlg.field_index;
                _events.field_name  = dlg.field_name;
                _events.value_index = dlg.value_index;
                _events.value_name  = dlg.value_name;
                _events.value_input = dlg.value_input;

                _events.enable_filter(true);
            }

            if(!_dbgview.init([&](DWORD pid, const char* str) {
                auto p = (LogDataUI*)::SendMessage(_hwnd, kLogCmd, LogMessage::Alloc, 0);
                p = LogDataUI::from_dbgview(pid, str, p);
                ::PostMessage(_hwnd, kLogCmd, LogMessage::Log, LPARAM(p));
            }))
            {
                async_call([&]() {
                    msgbox(L"无法启动 DebugView 日志，当前可能有其它的 DebugView 日志查看器正在运行。", MB_ICONINFORMATION);
                    close();
                });
            }
        });

        return 0;
    }
    case WM_SIZE:
    {
        break;
    }
    case WM_NCACTIVATE:
    {
        if(wparam == FALSE && _tipwnd->showing()) {
            return FALSE;
        }
        break;
    }
    case WM_CLOSE:
    {
        ::GetWindowRect(_hwnd, &winpos);

        _dbgview.uninit();

        DestroyWindow(_hwnd);

        return 0;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT MiniView::on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr)
{
    if (!pc) {
        if (hwnd == _listview->get_header()) {
            if (code == HDN_ENDTRACK) {
                return _on_drag_column(hdr);
            }
        }

        return 0;
    }

    if(pc == _listview) {
        if(code == LVN_GETDISPINFO) {
            return _on_get_dispinfo(hdr);
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
        break;
        auto& log = *_events[lvcd->nmcd.dwItemSpec];
        // lvcd->clrText = _colors[log.level].fg;
        // lvcd->clrTextBk = _colors[log.level].bg;
        lr = CDRF_NEWFONT;
        break;
    }

    return lr;
}

LRESULT MiniView::_on_get_dispinfo(NMHDR * hdr)
{
    auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
    auto& evt = _events[pdi->item.iItem];
    auto lit = &pdi->item;
    auto field = (*evt)[pdi->item.iSubItem];

    lit->pszText = const_cast<LPWSTR>(field);

    return 0;
}

LRESULT MiniView::_on_drag_column(NMHDR * hdr)
{
    auto nmhdr = (NMHEADER*)hdr;
    auto& item = nmhdr->pitem;
    auto& col = _columns[nmhdr->iItem];

    col.show = item->cxy != 0;
    if (item->cxy) col.width = item->cxy;

    return 0;
}

LRESULT MiniView::control_message(taowin::syscontrol* ctl, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    if(ctl == _listview) {
        // TODO static!!
        static bool mi = false;

        if(umsg == WM_MOUSEMOVE) {
            if(!mi) {
                TRACKMOUSEEVENT tme = {0};
                tme.cbSize = sizeof(tme);
                tme.hwndTrack = _listview->hwnd();
                tme.dwFlags = TME_HOVER | TME_LEAVE;
                tme.dwHoverTime = HOVER_DEFAULT;
                _TrackMouseEvent(&tme);
                mi = true;
            }
        }
        else if(umsg == WM_MOUSELEAVE) {
            mi = false;
        }
        if(umsg == WM_MOUSEHOVER) {
            mi = false;

            LVHITTESTINFO hti;
            hti.pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

            if(_listview->subitem_hittest(&hti) != -1/* && hti.iSubItem == 9*/) {
                // TODO 界面与逻辑应该是要分离的啊！！
                NMLVDISPINFO info;
                info.item.iItem = hti.iItem;
                info.item.iSubItem = hti.iSubItem;
                _on_get_dispinfo((NMHDR*)&info);

                bool need_tip = false;
                auto text = info.item.pszText;

                if(!need_tip) {
                    if(wcschr(text, L'\n')) {
                        need_tip = true;
                    }
                }

                // 不初始化会报潜在使用了未初始化的变量（但实际上不可能）
                int text_width = 0;
                constexpr int text_padding = 20;

                if(!need_tip) {
                    HDC hdc = ::GetDC(_listview->hwnd());
                    HFONT hFont = (HFONT)::SendMessage(_listview->hwnd(), WM_GETFONT, 0, 0);
                    HFONT hOldFont = SelectFont(hdc, hFont);

                    SIZE szText = {0};
                    if(::GetTextExtentPoint32(hdc, text, wcslen(text), &szText)) {
                        int col_width = _columns[hti.iSubItem].width;
                        text_width = szText.cx + text_padding;

                        if(text_width > col_width) {
                            need_tip = true;
                        }
                    }

                    SelectFont(hdc, hOldFont);

                    ::ReleaseDC(_listview->hwnd(), hdc);
                }

                if(!need_tip) {
                    taowin::Rect rcSubItem, rcListView;
                    ::GetClientRect(_listview->hwnd(), &rcListView);

                    if(_listview->get_subitem_rect(0, hti.iSubItem, &rcSubItem)) {
                        if(rcSubItem.left < rcListView.left || rcSubItem.left + text_width > rcListView.right) {
                            need_tip = true;
                        }
                    }
                }

                if(need_tip && !_tipwnd->showing()) {
                    _tipwnd->popup(text, _mgr.get_font(L"default"));
                }
            }

            return 0;
        }
    }
    return __super::control_message(ctl, umsg, wparam, lparam);
}

}
