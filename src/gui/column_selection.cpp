#include "stdafx.h"

#include "misc/config.h"

#include "column_selection.h"

namespace taoetw {

void ColumnSelection::get_metas(WindowMeta * metas)
{
    __super::get_metas(metas);
    metas->style &= ~(WS_MINIMIZEBOX);
}

LPCTSTR ColumnSelection::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="±íÍ·" size="512,480">
    <res>
        <font name="default" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="1" face="Î¢ÈíÑÅºÚ" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical name="container" padding="20,20,20,20">

        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT ColumnSelection::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        auto container = _root->find<taowin::vertical>(L"container");

        int valid_index = 0;

        for (int i = 0; i < (int)_columns.size(); i++) {
            if(!_columns[i].valid) continue;

            auto check = new taowin::check;
            std::map<taowin::string, taowin::string> attrs;

            attrs[_T("text")] = _columns[i].name.c_str();
            attrs[_T("name")] = std::to_wstring(valid_index).c_str();
            attrs[_T("checked")] = _columns[i].show ? _T("true") : _T("false");

            check->create(_hwnd, attrs, _mgr);
            ::SetWindowLongPtr(check->hwnd(), GWL_USERDATA, LONG(check));
            container->add(check);

            valid_index++;
        }

        // Ê¼ÖÕÑ¡ÔñµÚ1¸ö£¨½ûÖ¹ÐÞ¸Ä£©
        if(_columns.size()) {
            container->operator[](0)->set_enabled(false);
        }

        taowin::Rect rc{ 0, 0, 200, (valid_index+1) * 20 + 50 };
        ::AdjustWindowRectEx(&rc, ::GetWindowLongPtr(_hwnd, GWL_STYLE), !!::GetMenu(_hwnd), ::GetWindowLongPtr(_hwnd, GWL_EXSTYLE));
        ::SetWindowPos(_hwnd, nullptr, 0, 0, rc.width(), rc.height(), SWP_NOMOVE | SWP_NOZORDER);

        return 0;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ColumnSelection::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    int id = -1;
    ::swscanf(pc->name().c_str(), L"%d", &id);

    if (id >= 0 && id < int(_columns.size())) {
        if (code == BN_CLICKED) {
            int index = _ttoi(pc->name().c_str());
            _on_toggle(index);
            return 0;
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////

void ColumnManager::update()
{
    _available_indices.clear();
    _showing_indices.clear();

    int i = 0;

    for(auto& c : _columns) {
        if(c.valid) {
            _available_indices.emplace_back(i);

            if(c.show) {
                _showing_indices.emplace_back(i);
            }
        }

        i++;
    }
}

void ColumnManager::for_each(ColumnFlags::Value f, std::function<void(int i, Column& c)> fn)
{
    int i = 0;

    switch(f)
    {
    case ColumnFlags::All:
        for(auto& c : _columns) {
            fn(i++, c);
        }
        break;

    case  ColumnFlags::Available:
        for(auto& p : _available_indices) {
            fn(i++, _columns[p]);
        }
        break;

    case  ColumnFlags::Showing:
        for(auto& p : _showing_indices) {
            fn(i++, _columns[p]);
        }
        break;

    default:
        assert("invalid flags" && 0);
        break;
    }
}

void ColumnManager::show(int available_index, int* listview_index)
{
    if(available_index < 0 || available_index >= (int)_available_indices.size())
        return;

    auto ai = _available_indices[available_index];
    auto& c = _columns[ai];

    if(!c.valid || c.show)
        return;

    int pos = 0;
    for(auto p : _showing_indices) {
        if(p > ai)
            break;
        pos++;
    }

    *listview_index = pos;
    _showing_indices.emplace(_showing_indices.cbegin() + pos, ai);
}

void ColumnManager::hide(bool is_listview_index, int index, int* listview_delete_index)
{
    int ai = is_listview_index ? _showing_indices[index] : _available_indices[index];

    int show_index = 0;

    for(auto p : _showing_indices) {
        if(p == ai) {
            break;
        }
        show_index++;
    }

    if(!is_listview_index) *listview_delete_index = show_index;
    _showing_indices.erase(_showing_indices.cbegin() + show_index);
}

} // namespace taoetw
