#include "stdafx.h"

#include "misc/event_system.hpp"
#include "misc/config.h"

#include "_logdata_define.hpp"
#include "_module_entry.hpp"
#include "event_container.h"
#include "tooltip_window.h"
#include "result_filter.h"


namespace taoetw {

void ResultFilter::get_metas(WindowMeta * metas)
{
    __super::get_metas(metas);
    metas->style &= ~(WS_MINIMIZEBOX);
}

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
        _tipwnd->create(this);
        _tipwnd->set_font(_mgr.get_font(L"default"));

        if(_currnet_project) {
            std::wstring t(L"结果过滤");
            t += L"（模块：" + _currnet_project->name + L"）";
            ::SetWindowText(_hwnd, t.c_str());
        }

        _listview   = _root->find<taowin::listview>(L"list");
        _btn_add    = _root->find<taowin::button>(L"add");
        _btn_delete = _root->find<taowin::button>(L"delete");
        _btn_all    = _root->find<taowin::button>(L"all");

        _listview->insert_column(L"名字", 100, 0);
        _listview->insert_column(L"字段", 60, 1);
        _listview->insert_column(L"文本", 180, 2);

        _listview->set_item_count((int)_filters.size(), 0);

        subclass_control(_listview);

        return 0;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ResultFilter::control_message(taowin::syscontrol* ctl, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    // TODO static!!
    static bool mi = false;

    if(umsg == WM_MOUSEMOVE) {
        if(!mi) {
            taowin::set_track_mouse(ctl);
            mi = true;
        }
    }
    else if(umsg == WM_MOUSELEAVE) {
        mi = false;
    }
    else if(umsg == WM_MOUSEHOVER) {
        mi = false;

        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

        if(ctl == _listview) {
            LVHITTESTINFO hti;;
            hti.pt = pt;
            if(_listview->subitem_hittest(&hti) != -1) {
                _tipwnd->format(_filters[hti.iItem]->to_tip());
            }
            return 0;
        }
    }

    return __super::control_message(ctl, umsg, wparam, lparam);
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
            case 1: value = flt.field_name.c_str();  break;
            case 2: value = !flt.value_input.empty() ? flt.value_input.c_str() : flt.value_name.c_str(); break;
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
                g_evtsys.trigger(L"filter:set", _filters[nmlv->iItem]);
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
        if(code == BN_CLICKED) {
            AddNewFilter dlg(_on_get_fields, _get_value_list);
            if(dlg.domodal(this) == IDOK) {
                _onnewfilter(dlg.name, dlg.field_index, dlg.field_name, dlg.value_index, dlg.value_name, dlg.value_input);
                _listview->set_item_count(_filters.size(), LVSICF_NOINVALIDATEALL);

                int index = _listview->get_item_count() - 1;
                _listview->ensure_visible(index);   // 确保可见
                _listview->set_item_state(-1, LVIS_SELECTED, 0); //取消选中其它的
                _listview->set_item_state(index, LVIS_SELECTED, LVIS_SELECTED); //选中当前新增的
                _listview->focus();
            }
        }
    }
    else if (pc == _btn_delete) {
        if(code == BN_CLICKED) {
            std::vector<int> items;
            _listview->get_selected_items(&items);

            for(auto it = items.crbegin(); it != items.crend(); ++it) {
                g_evtsys.trigger(L"filter:del", *it);
            }

            _listview->set_item_count((int)_filters.size(), 0);
            _listview->redraw_items(0, _listview->get_item_count());

            items.clear();
            _listview->get_selected_items(&items);
            _btn_delete->set_enabled(!items.empty());
        }
    }
    else if (pc == _btn_all) {
        if(code == BN_CLICKED) {
            g_evtsys.trigger(L"filter:set", nullptr);
            close(0);
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////

void AddNewFilter::get_metas(WindowMeta * metas)
{
    __super::get_metas(metas);
    metas->style &= ~(WS_MINIMIZEBOX);
}

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
                    <label style="centerimage" text="字段" width="50"/>
                    <combobox name="field-name" style="tabstop" height="200"/>
                </horizontal>
                <horizontal height="30" padding="0,3,0,3">
                    <label style="centerimage" text="文本" width="50"/>
                    <container>
                        <edit name="value-input" text="" style="tabstop" exstyle="clientedge" />
                        <vertical name="不加这个，下面的combobox显示不出来">
                            <combobox name="value-name-1" style="tabstop,droplist" height="200" />
                        </vertical>
                        <vertical name="不加这个，下面的combobox显示不出来">
                            <combobox name="value-name-2" style="tabstop,dropdown" height="200" />
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
        _name        = _root->find<taowin::edit>(L"name");
        _field_name  = _root->find<taowin::combobox>(L"field-name");
        _value_name_1= _root->find<taowin::combobox>(L"value-name-1");
        _value_name_2= _root->find<taowin::combobox>(L"value-name-2");
        _value_input = _root->find<taowin::edit>(L"value-input");

        _save        = _root->find<taowin::button>(L"save");
        _cancel      = _root->find<taowin::button>(L"cancel");

        int def = 0;
        _on_get_fields(&_fields, &def);

        for (auto& field : _fields)
            _field_name->add_string(field.c_str());

        _field_name->set_cur_sel(def);
        // TODO
        on_notify(_field_name->hwnd(), _field_name, CBN_SELCHANGE, nullptr);

        return 0;
    }
    }

    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT AddNewFilter::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    if (pc == _save) {
        if(code == BN_CLICKED) {
            _on_save();
        }
    }
    else if (pc == _cancel) {
        if(code == BN_CLICKED) {
            close(IDCANCEL);
        }
    }
    else if (pc == _field_name) {
        if (code == CBN_SELCHANGE) {
            int sel = _field_name->get_cur_sel();

            _values.clear();
            _get_values(sel, &_values, &_value_editable);
            _change_value_editable();

            _value_input->set_visible(_values.empty());
            _value_name->set_visible(!_values.empty());

            if (!_values.empty()) {
                _value_name->reset_content();

                for (auto& pair : _values) {
                    _value_name->add_string(pair.second);
                }

                _value_name->set_cur_sel(0);
                _value_name->focus();
            }
            else {
                _value_input->focus();
            }
        }
    }
    else if(pc == _value_name) {
        if(code == CBN_SELCHANGE) {
            DBG("%s", L"selchange");
        }
        else if(code == CBN_EDITCHANGE) {
            DBG("%s", L"editchange");
        }
        else if(code == CBN_EDITUPDATE) {
            DBG("%s", L"editupdate");
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

void AddNewFilter::_change_value_editable()
{
    _value_name_1->set_visible(!_value_editable);
    _value_name_2->set_visible(_value_editable);
    _value_name = _value_editable ? _value_name_2 : _value_name_1;
}

int AddNewFilter::_on_save()
{
    bool iscbo = _value_name->is_visible();

    try {
        if (iscbo) {
            // 可编辑，并且编辑过
            auto sel = _value_name->get_cur_sel();
            if(_value_editable && sel == -1) {
                value_input = _value_name->get_text();
                if(value_input.empty()) throw 0;
                value_index = -1;
                value_name = L"";
            }
            else {
                value_input = L"";
                value_index = _values[sel].first;
                value_name = _values[sel].second;
            }
        }
        else {
            value_index = -1;
            value_input = _value_input->get_text();
            if (value_input.empty()) throw 0;
        }

        name = _name->get_text();
        field_index = _field_name->get_cur_sel();
        field_name = _fields[field_index];
        close(IDOK);
    }
    catch (...) {
        _value_input->focus();
    }

    return 0;
}

}
