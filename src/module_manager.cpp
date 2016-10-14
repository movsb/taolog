#include "module_manager.h"

#include "module_editor.h"

namespace taoetw {

LPCTSTR ModuleManager::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="模块管理" size="300,300">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <horizontal padding="5,5,5,5">
            <listview name="list" style="showselalways,ownerdata" exstyle="clientedge" />
            <vertical padding="5,5,5,5" width="50">
                <button name="add" text="添加" height="24" />
                <control height="20" />
                <button name="modify" text="修改" style="disabled" height="24"/>
                <control height="5" />
                <button name="enable" text="启用" style="disabled" height="24" />
                <control height="5" />
                <button name="delete" text="删除" style="disabled" height="24"/>
                <control height="5" />
            </vertical>
        </horizontal>
    </root>
</window>
)tw";
    return json;
}

LRESULT ModuleManager::handle_message(UINT umsg, WPARAM wparam, LPARAM lparam)
{
    switch (umsg) {
    case WM_CREATE:
    {
        _listview = _root->find<taowin::listview>(L"list");

        _listview->insert_column(L"名字", 150, 0);
        _listview->insert_column(L"状态", 50, 1);

        _listview->set_item_count((int)_modules.size(), 0);

        return 0;
    }

    }
    return __super::handle_message(umsg, wparam, lparam);
}

LRESULT ModuleManager::on_notify(HWND hwnd, taowin::control * pc, int code, NMHDR * hdr)
{
    if (!pc) return 0;

    if (pc->name() == _T("list")) {
        if (code == LVN_GETDISPINFO) {
            auto pdi = reinterpret_cast<NMLVDISPINFO*>(hdr);
            auto& mod = _modules[pdi->item.iItem];
            auto lit = &pdi->item;

            const TCHAR* value = _T("");

            switch (lit->iSubItem) {
            case 0: value = mod->name.c_str();                   break;
            case 1: value = mod->enable ? L"已启用" : L"已禁用";  break;
            }

            lit->pszText = const_cast<TCHAR*>(value);

            return 0;
        }
        else if (code == NM_CUSTOMDRAW) {
            LRESULT lr = CDRF_DODEFAULT;
            ModuleEntry* mod;

            auto lvcd = (LPNMLVCUSTOMDRAW)hdr;

            switch (lvcd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                lr = CDRF_NOTIFYITEMDRAW;
                break;

            case CDDS_ITEMPREPAINT:
                mod = _modules[lvcd->nmcd.dwItemSpec];

                if (mod->enable) {
                    lvcd->clrText = RGB(0, 0, 255);
                    lvcd->clrTextBk = RGB(255, 255, 255);
                }
                else {
                    lvcd->clrText = RGB(0, 0, 0);
                    lvcd->clrTextBk = RGB(255, 255, 255);
                }

                lr = CDRF_NEWFONT;
                break;
            }

            return lr;
        }
        else if (code == NM_DBLCLK) {
            auto nmlv = reinterpret_cast<NMITEMACTIVATE*>(hdr);

            if (nmlv->iItem != -1) {
                _toggle_enable(nmlv->iItem);
            }

            return 0;
        }
        else if (code == LVN_ITEMCHANGED
            || code == LVN_DELETEITEM
            || code == LVN_DELETEALLITEMS
            ) {
            auto btn_enable = _root->find<taowin::button>(L"enable");
            auto btn_modify = _root->find<taowin::button>(L"modify");
            auto btn_delete = _root->find<taowin::button>(L"delete");

            int sel_count = _listview->get_selected_count();
            int i = _listview->get_next_item(-1, LVNI_SELECTED);

            btn_enable->set_enabled(sel_count != 0);
            btn_modify->set_enabled(sel_count == 1 && i != -1 && !_modules[i]->enable);
            btn_delete->set_enabled(sel_count != 0);

            if (sel_count == 1) {
                btn_enable->set_text(_modules[i]->enable ? L"禁用" : L"启用");
            }

            return 0;
        }
    }
    else if (pc->name() == L"enable") {
        int index = _listview->get_next_item(-1, LVNI_SELECTED);
        if (index != -1)
            _toggle_enable(index);
        return 0;
    }
    else if (pc->name() == L"add") {
        _add_item();
        return 0;
    }
    else if (pc->name() == L"modify") {
        int index = _listview->get_next_item(-1, LVNI_SELECTED);
        if (index != -1)
            _modify_item(index);
        return 0;
    }
    else if (pc->name() == L"delete") {
        _delete_item();
        return 0;
    }

    return 0;
}

void ModuleManager::_toggle_enable(int i)
{
    _modules[i]->enable = !_modules[i]->enable;
    _listview->redraw_items(i, i);

    _root->find<taowin::button>(L"enable")->set_text(_modules[i]->enable ? L"禁用" : L"启用");
    _root->find<taowin::button>(L"modify")->set_enabled(!_modules[i]->enable);

    // TODO fire event
}

void ModuleManager::_modify_item(int i)
{
    auto onok = [&](ModuleEntry* entry) {
        _listview->redraw_items(i, i);
        // TODO fire event
    };

    auto onguid = [&](const GUID& guid, std::wstring* err) {
        if (_has_guid(guid) && !::IsEqualGUID(guid, _modules[i]->guid)) {
            *err = L"此 GUID 已经存在。";
            return false;
        }

        return true;
    };

    (new ModuleEntryEditor(_modules[i], onok, onguid))->domodal(this);
}

void ModuleManager::_delete_item()
{
    int count = _listview->get_selected_count();
    const wchar_t* title = count == 1 ? _modules[_listview->get_next_item(-1, LVNI_SELECTED)]->name.c_str() : L"确认";

    if (msgbox((L"确定要删除选中的 " + std::to_wstring(count) + L" 项？").c_str(), MB_OKCANCEL | MB_ICONQUESTION, title) == IDOK) {
        int index = -1;
        std::vector<int> selected;
        while ((index = _listview->get_next_item(index, LVNI_SELECTED)) != -1) {
            selected.push_back(index);
        }

        for (auto it = selected.crbegin(); it != selected.crend(); it++) {
            _listview->delete_item(*it);
            _modules.erase(_modules.begin() + *it);
        }
    }

    // TODO fire event
}

void ModuleManager::_add_item()
{
    auto onok = [&](ModuleEntry* entry) {
        _modules.push_back(entry);
        _listview->set_item_count((int)_modules.size(), LVSICF_NOINVALIDATEALL);
    };

    auto onguid = [&](const GUID& guid, std::wstring* err) {
        if (_has_guid(guid)) {
            *err = L"此 GUID 已经存在。";
            return false;
        }

        return true;
    };

    (new ModuleEntryEditor(nullptr, onok, onguid))->domodal(this);

    // TODO fire event
}

bool ModuleManager::_has_guid(const GUID & guid)
{
    for (auto& mod : _modules) {
        if (::IsEqualGUID(guid, mod->guid)) {
            return true;
        }
    }

    return false;
}

}
