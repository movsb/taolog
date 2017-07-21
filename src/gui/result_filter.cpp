#include "stdafx.h"

#include "misc/event_system.hpp"
#include "misc/config.h"

#include "logdata_define.h"
#include "_module_entry.hpp"
#include "event_container.h"
#include "tooltip_window.h"
#include "result_filter.h"


namespace taolog {

void ResultFilter::get_metas(WindowMeta * metas)
{
    __super::get_metas(metas);
    metas->style &= ~(WS_MINIMIZEBOX);
}

LPCTSTR ResultFilter::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<Window title="结果过滤" size="420,300">
    <Resource>
        <Font name="default" face="微软雅黑" size="12"/>
        <Font name="1" face="微软雅黑" size="12"/>
        <Font name="consolas" face="Consolas" size="12"/>
    </Resource>
    <Root>
        <Vertical padding="5,5,5,5">
            <Horizontal>
                <Vertical>
                    <Label text="已有过滤器（双击以使用）：" height="18" />
                    <ListView name="list" style="ownerdata,showselalways,tabstop" exstyle="clientedge" />
                </Vertical>
                <Control width="5" />
                <Vertical width="50">
                    <Control height="18" />
                    <Button name="all" text="全部" height="24" style="tabstop" />
                    <Control height="10" />
                    <Button name="add" text="添加" height="24" style="tabstop"/>
                    <Control height="5" />
                    <Button name="delete" text="删除" style="disabled,tabstop" height="24"/>
                    <Control height="5" />
                </Vertical>
            </Horizontal>
        </Vertical>
    </Root>
</Window>
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

        _listview   = _root->find<taowin::ListView>(L"list");
        _btn_add    = _root->find<taowin::Button>(L"add");
        _btn_delete = _root->find<taowin::Button>(L"delete");
        _btn_all    = _root->find<taowin::Button>(L"all");

        _listview->insert_column(L"名字", 100, 0);
        _listview->insert_column(L"字段", 50,  1);
        _listview->insert_column(L"文本", 130, 2);
        _listview->insert_column(L"临时", 50,  3);

        subclass_control(_listview);

        _data_source.SetFilters(&_filters);
        _listview->set_source(&_data_source);

        return 0;
    }
    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ResultFilter::control_message(taowin::SystemControl* ctl, UINT umsg, WPARAM wparam, LPARAM lparam)
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

LRESULT ResultFilter::on_notify(HWND hwnd, taowin::Control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    if (pc == _listview) {
        if (code == LVN_ITEMCHANGED) {
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
            auto onnew = [&](const std::wstring& name,
                int field_index, const std::wstring& field_name, int value_index,
                const std::wstring& value_name, const std::wstring& value_input)
            {
                _onnewfilter(name, field_index, field_name, value_index, value_name, value_input);

                _listview->update_source(LVSICF_NOINVALIDATEALL);

                int index = _listview->get_item_count() - 1;
                _listview->ensure_visible(index);   // 确保可见
                _listview->set_item_state(-1, LVIS_SELECTED, 0); //取消选中其它的
                _listview->set_item_state(index, LVIS_SELECTED, LVIS_SELECTED); //选中当前新增的
                _listview->focus();
            };

            AddNewFilter dlg(_on_get_fields, _get_value_list, onnew);
            dlg.domodal(this);
        }
    }
    else if (pc == _btn_delete) {
        if(code == BN_CLICKED) {
            std::vector<int> items;
            _listview->get_selected_items(&items);

            for(auto it = items.crbegin(); it != items.crend(); ++it) {
                g_evtsys.trigger(L"filter:del", *it);
            }

            _listview->update_source();
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
<Window title="添加过滤" size="350,160">
    <Resource>
        <Font name="default" face="微软雅黑" size="12"/>
        <Font name="1" face="微软雅黑" size="12"/>
        <Font name="consolas" face="Consolas" size="12"/>
    </Resource>
    <Root>
        <Vertical>
            <Vertical name="container" padding="10,10,10,10" height="108">
                <Horizontal height="30" padding="0,3,0,3">
                    <Label style="centerimage" text="名字" width="50"/>
                    <TextBox name="name" text="" style="tabstop" exstyle="clientedge"/>
                </Horizontal>
                <Horizontal height="30" padding="0,3,0,3">
                    <Label style="centerimage" text="字段" width="50"/>
                    <ComboBox name="field-name" style="tabstop" height="200"/>
                </Horizontal>
                <Horizontal height="30" padding="0,3,0,3">
                    <Label style="centerimage" text="文本" width="50"/>
                    <Container>
                        <TextBox name="value-input" text="" style="tabstop" exstyle="clientedge" />
                        <Vertical name="不加这个，下面的ComboBox显示不出来">
                            <ComboBox name="value-name-1" style="tabstop,droplist" height="200" />
                        </Vertical>
                        <Vertical name="不加这个，下面的ComboBox显示不出来">
                            <ComboBox name="value-name-2" style="tabstop,dropdown" height="200" />
                        </Vertical>
                        <Vertical name="不加这个，下面的ComboBox显示不出来">
                            <ComboBox name="value-name-3" style="tabstop,dropdown,vscroll,ownerdrawfixed,hasstrings" height="200" />
                        </Vertical>
                    </Container>
                </Horizontal>
            </Vertical>
            <Horizontal height="40" padding="10,4,10,4">
                <Control />
                <Button name="save" text="保存" width="50" style="tabstop,default"/>
                <Control width="10" />
                <Button name="cancel" text="取消" width="50" style="tabstop"/>
            </Horizontal>
        </Vertical>
    </Root>
</Window>
)tw";
    return json;
}

LRESULT AddNewFilter::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch(umsg)
    {
    case WM_CREATE:
    {
        _name        = _root->find<taowin::TextBox>(L"name");
        _field_name  = _root->find<taowin::ComboBox>(L"field-name");
        _value_name_1= _root->find<taowin::ComboBox>(L"value-name-1");
        _value_name_2= _root->find<taowin::ComboBox>(L"value-name-2");
        _value_name_3= _root->find<taowin::ComboBox>(L"value-name-3");
        _value_input = _root->find<taowin::TextBox>(L"value-input");

        _save        = _root->find<taowin::Button>(L"save");
        _cancel      = _root->find<taowin::Button>(L"cancel");

        int def = 0;
        _on_get_fields(&_fields, &def);

        for (auto& field : _fields)
            _field_name->add_string(field.c_str());

        _field_name->set_cur_sel(def);
        // TODO
        on_notify(_field_name->hwnd(), _field_name, CBN_SELCHANGE, nullptr);

        return 0;
    }
    case WM_DRAWITEM:
    {
        unsigned int id = unsigned int(wparam);
        if(id == _value_name_3->get_ctrl_id()) {
            auto dis = (DRAWITEMSTRUCT*)lparam;
            _value_name_3->drawit(dis);
            return TRUE;
        }
        break;
    }
    }

    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT AddNewFilter::on_notify(HWND hwnd, taowin::Control * pc, int code, NMHDR * hdr)
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
            _get_values(sel, &_values, &_value_editable, &_draw_value);
            _value_name_3->set_ondraw(_draw_value);
            _change_value_control();

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
            DBG("%s: %d", L"selchange", _value_name->get_cur_sel());
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

void AddNewFilter::_change_value_control()
{
    _value_name_1->set_visible(!_value_editable);
    _value_name_2->set_visible(_value_editable && !_draw_value);
    _value_name_3->set_visible(_value_editable && _draw_value);
    _value_name = !_value_editable ? _value_name_1 : ( !_draw_value ? _value_name_2 : _value_name_3);
}

int AddNewFilter::_on_save()
{
    bool iscbo = _value_name->is_visible();

    try {
        if(iscbo) {
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
            if(value_input.empty()) throw 0;
        }
    }
    catch(int) {
        _value_input->focus();
        return 0;
    }

    name = _name->get_text();
    field_index = _field_name->get_cur_sel();
    field_name = _fields[field_index];

    try {
        _on_new(name, field_index, field_name, value_index, value_name, value_input);
        close(IDOK);
    }
    catch(const std::wstring& err) {
        msgbox(err, MB_ICONERROR, L"错误");
        if(iscbo) _value_name->focus();
        else _value_input->focus();
    }

    return 0;
}

}
