#include "result_filter.h"

namespace taoetw {

LPCTSTR ResultFilter::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="结果过滤" size="420,300">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical padding="5,5,5,5">
            <horizontal>
                <vertical>
                    <label text="已有过滤器：" height="18" />
                    <listview name="list" style="ownerdata,showselalways" exstyle="clientedge" />
                </vertical>
                <control width="5" />
                <vertical width="50">
                    <control height="18" />
                    <button name="all" text="全部" height="24" />
                    <control height="5" />
                    <button name="add" text="添加" height="24" />
                    <control height="5" />
                    <button name="delete" text="删除" style="disabled" height="24"/>
                    <control height="5" />
                </vertical>
            </horizontal>
        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT ResultFilter::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg)
    {
    case WM_CREATE:
    {
        _listview   = _root->find<taowin::listview>(L"list");
        _btn_add    = _root->find<taowin::button>(L"add");
        _btn_delete = _root->find<taowin::button>(L"delete");
        _btn_all    = _root->find<taowin::button>(L"all");

        _listview->insert_column(L"名字", 100, 0);
        _listview->insert_column(L"基于", 60, 1);
        _listview->insert_column(L"规则", 180, 2);

        _listview->set_item_count((int)_filters.size(), 0);

        return 0;
    }

    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ResultFilter::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    if (pc == _listview) {
        if (code == LVN_GETDISPINFO) {
            auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
            auto& flt = *_filters[pdi->item.iItem];
            auto lit = &pdi->item;

            const TCHAR* value = _T("");

            switch (lit->iSubItem) {
            case 0: value = flt.name.c_str();  break;
            case 1: value = flt.base.c_str();  break;
            case 2: value = flt.rule.c_str(); break;
            }

            lit->pszText = const_cast<TCHAR*>(value);

            return 0;
        }
        else if (code == LVN_ITEMCHANGED) {
            std::vector<int> items;
            _listview->get_selected_items(&items);

            _btn_delete->set_enabled(!items.empty());
        }
        else if (code == NM_DBLCLK) {
            auto nmlv = reinterpret_cast<NMITEMACTIVATE*>(hdr);

            if (nmlv->iItem != -1) {
                _on_set_filter(_filters[nmlv->iItem]);

                if(!(::GetAsyncKeyState(VK_LCONTROL) & 0x8000))
                    close(0);
            }
        }
    }
    else if (pc == _btn_add) {
        AddNewFilter dlg(_on_get_bases);
        if (dlg.domodal(this) == IDOK) {
            auto p = new EventContainer(dlg.name, dlg.base, dlg.rule, dlg.base_int);
            _on_add_new(p);
            _listview->set_item_count(_filters.size(), LVSICF_NOINVALIDATEALL);
        }
    }
    else if (pc == _btn_delete) {
        std::vector<int> items;
        _listview->get_selected_items(&items);

        for (auto it = items.crbegin(); it != items.crend(); ++it) {
            _on_delete(*it);
        }

        _listview->set_item_count((int)_filters.size(), 0);
        _listview->redraw_items(0, _listview->get_item_count());

        items.clear();
        _listview->get_selected_items(&items);
        _btn_delete->set_enabled(!items.empty());
    }
    else if (pc == _btn_all) {
        _on_set_filter(nullptr);

        if(!(::GetAsyncKeyState(VK_LCONTROL) & 0x8000))
            close(0);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////

LPCTSTR AddNewFilter::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="添加过滤" size="350,160">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <vertical>
            <vertical name="container" padding="10,10,10,10" height="108">
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="名字" width="50"/>
                    <edit name="name" text="(none)" style="tabstop" exstyle="clientedge" style=""/>
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="基于" width="50"/>
                    <combobox name="base" style="tabstop" height="200"/>
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="规则" width="50"/>
                    <edit name="rule" text="正则表达式（C++11 / Javascript）" style="tabstop" exstyle="clientedge" />
                </horizontal>
            </vertical>
            <horizontal height="40" padding="10,4,10,4">
                <control />
                <button name="save" text="保存" width="50"/>
                <control width="10" />
                <button name="cancel" text="取消" width="50"/>
            </horizontal>
        </vertical>
    </root>
</window>
)tw";
    return json;
}

LRESULT AddNewFilter::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch(umsg)
    {
    case WM_CREATE:
    {
        _name = _root->find<taowin::edit>(L"name");
        _base = _root->find<taowin::combobox>(L"base");
        _rule = _root->find<taowin::edit>(L"rule");

        _save = _root->find<taowin::button>(L"save");
        _cancel = _root->find<taowin::button>(L"cancel");

        _on_get_bases(&_bases);

        for (auto& base : _bases) {
            _base->add_string(base.c_str());
        }

        _base->set_cur_sel((int)_bases.size() - 1);

        _rule->focus();
        _rule->set_sel(0, -1);

        return 0;
    }
    }

    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT AddNewFilter::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    if (pc == _save) {
        try {
            rule = _rule->get_text();
            if (rule == L"") throw 0;

            std::wregex re(rule, std::regex_constants::icase);

            name = _name->get_text();
            base_int = _base->get_cur_sel();
            base = _bases[base_int];
            close(IDOK);
        }
        catch (...) {
            msgbox(L"无效正则表达式。", MB_ICONERROR);
        }
    }
    else if (pc == _cancel) {
        close(IDCANCEL);
    }
    return 0;
}

}
