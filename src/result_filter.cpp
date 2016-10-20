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
                    <label text="已有过滤器（双击以使用）：" height="18" />
                    <listview name="list" style="ownerdata,showselalways,tabstop" exstyle="clientedge" />
                </vertical>
                <control width="5" />
                <vertical width="50">
                    <control height="18" />
                    <button name="all" text="全部" height="24" style="tabstop" />
                    <control height="5" />
                    <button name="add" text="添加" height="24" style="tabstop"/>
                    <control height="5" />
                    <button name="delete" text="删除" style="disabled,tabstop" height="24"/>
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
            case 2: value = *flt.rule.c_str() ? flt.rule.c_str() : flt.str_base_value.c_str(); break;
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

                close(0);
            }
        }
        else if (code == NM_CUSTOMDRAW) {
            LRESULT lr = CDRF_DODEFAULT;
            auto lvcd = (LPNMLVCUSTOMDRAW)hdr;

            switch (lvcd->nmcd.dwDrawStage)
            {
            case CDDS_PREPAINT:
                lr = CDRF_NOTIFYITEMDRAW;
                break;

            case CDDS_ITEMPREPAINT:
                if (_current_filter == _filters[lvcd->nmcd.dwItemSpec]) {
                    lvcd->clrText = RGB(255, 255, 255);
                    lvcd->clrTextBk = RGB(0xff, 0x60, 0xd8);
                    lr = CDRF_NEWFONT;
                }
                break;
            }

            return lr;
        }
    }
    else if (pc == _btn_add) {
        AddNewFilter dlg(_on_get_bases, _get_value_list);
        if (dlg.domodal(this) == IDOK) {
            _on_add_new(new EventContainer(dlg.name, dlg.base, dlg.rule, dlg.base_int, dlg.rule2, dlg.str_base_value));
            _listview->set_item_count(_filters.size(), LVSICF_NOINVALIDATEALL);

            int index = _listview->get_item_count() - 1;
            _listview->ensure_visible(index);   // 确保可见
            _listview->set_item_state(-1, LVIS_SELECTED, 0); //取消选中其它的
            _listview->set_item_state(index, LVIS_SELECTED, LVIS_SELECTED); //选中当前新增的
            _listview->focus();
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
                    <edit name="name" text="(none)" style="tabstop" exstyle="clientedge"/>
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="基于" width="50"/>
                    <combobox name="base" style="tabstop" height="200"/>
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="规则" width="50"/>
                    <container>
                        <edit name="rule" text="正则表达式（C++11 / Javascript）" style="tabstop" exstyle="clientedge" />
                        <vertical name="不加这个，下面的combobox显示不出来">
                            <combobox name="rule2" style="tabstop" height="200" />
                        </vertical>
                    </container>
                </horizontal>
            </vertical>
            <horizontal height="40" padding="10,4,10,4">
                <control />
                <button name="save" text="保存" width="50" style="tabstop,default"/>
                <control width="10" />
                <button name="cancel" text="取消" width="50" style="tabstop"/>
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
        _rule2 = _root->find<taowin::combobox>(L"rule2");

        _save = _root->find<taowin::button>(L"save");
        _cancel = _root->find<taowin::button>(L"cancel");

        // 界面库暂时不支持配置隐藏，所以先写在这里
        _rule2->set_visible(false);

        _on_get_bases(&_bases);

        for (auto& base : _bases) {
            _base->add_string(base.c_str());
        }

        _base->set_cur_sel(0);
        // TODO
        on_notify(_base->hwnd(), _base, CBN_SELCHANGE, nullptr);

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
        _on_save();
    }
    else if (pc == _cancel) {
        close(IDCANCEL);
    }
    else if (pc == _base) {
        if (code == CBN_SELCHANGE) {
            int sel = _base->get_cur_sel();

            _values.clear();
            _get_values(sel, &_values);

            _rule2->set_visible(!_values.empty());
            _rule->set_visible(_values.empty());

            if (!_values.empty()) {
                _rule2->reset_content();

                for (auto& pair : _values) {
                    int i = _rule2->add_string(pair.second);
                    _rule2->set_item_data(i, (void*)pair.first);
                }

                _rule2->set_cur_sel(0);
                _rule2->focus();
            }
            else {
                _rule->focus();
            }
        }
    }

    return 0;
}

bool AddNewFilter::filter_special_key(int vk)
{
    if (vk == VK_RETURN) {
        _on_save();
        return true;
    }

    return __super::filter_special_key(vk);
}

int AddNewFilter::_on_save()
{
    bool iscbo = _rule2->is_visible();

    try {
        if (iscbo) {
            rule = L"";
            rule2 = (int)_rule2->get_item_data(_rule2->get_cur_sel());
            str_base_value = _values[rule2];
        }
        else {
            rule = _rule->get_text();
            if (rule == L"") throw 0;
            std::wregex re(rule, std::regex_constants::icase);
            str_base_value = L"";
        }

        name = _name->get_text();
        base_int = _base->get_cur_sel();
        base = _bases[base_int];
        close(IDOK);
    }
    catch (...) {
        msgbox(L"无效正则表达式。", MB_ICONERROR);
        _rule->focus();
    }

    return 0;
}

}
