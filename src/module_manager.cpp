#include "module_manager.h"

#include "module_editor.h"

namespace taoetw {

LPCTSTR ModuleManager::get_skin_xml() const
{
    LPCTSTR json = LR"tw(
<window title="模块管理" size="280,300">
    <res>
        <font name="default" face="微软雅黑" size="12"/>
        <font name="1" face="微软雅黑" size="12"/>
        <font name="consolas" face="Consolas" size="12"/>
    </res>
    <root>
        <horizontal padding="5,5,5,5">
            <listview name="list" style="showselalways,ownerdata,tabstop" exstyle="clientedge" />
            <vertical padding="5,5,5,5" width="50">
                <button name="add" text="添加" height="24" style="tabstop" />
                <control height="20" />
                <button name="modify" text="修改" style="disabled,tabstop" height="24"/>
                <control height="5" />
                <button name="enable" text="启用" style="disabled,tabstop" height="24" />
                <control height="5" />
                <button name="delete" text="删除" style="disabled,tabstop" height="24"/>
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
        _btn_add    = _root->find<taowin::button>(L"add");
        _btn_enable = _root->find<taowin::button>(L"enable");
        _btn_modify = _root->find<taowin::button>(L"modify");
        _btn_delete = _root->find<taowin::button>(L"delete");
        _listview   = _root->find<taowin::listview>(L"list");

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
                    lvcd->clrTextBk = RGB(255, 255, 255);
                    lvcd->clrText = RGB(0xff, 0x60, 0xd8);
                    lr = CDRF_NEWFONT;
                }
                break;
            }

            return lr;
        }
        else if (code == NM_DBLCLK) {
            auto nmlv = reinterpret_cast<NMITEMACTIVATE*>(hdr);

            if (nmlv->iItem != -1) {
                std::vector<int> items{ nmlv->iItem };
                _enable_items(items, -1);
            }

            return 0;
        }
        else if (code == LVN_ITEMCHANGED) {
            _on_item_state_change();
            return 0;
        }
    }
    else if (pc->name() == L"enable") {
        std::vector<int> items;

        if (_listview->get_selected_items(&items)) {
            int state = _get_enable_state_for_items(items);

            // 说明选中了多个不同状态的项
            if (state == -1) {
                int choice = msgbox(L"【是】启用这些模块；\n【否】禁用这些模块。", MB_ICONQUESTION | MB_YESNOCANCEL);
                if (choice == IDCANCEL) return 0;
                state = choice == IDYES;
            }
            else {
                state = state == 1 ? 0 : 1;
            }

            _enable_items(items, state);
        }

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
        std::vector<int> items;

        if (!_listview->get_selected_items(&items))
            return 0;

        // 在全部为禁用状态的时候进行提示（不全部为禁用时后边会提示哪些处于启用状态，此提示就不必要了）
        const wchar_t* title = items.size()==1 ? _modules[items[0]]->name.c_str() : L"确认";
        int state = _get_enable_state_for_items(items);
        if (state == 0 && msgbox((L"确定要删除选中的 " + std::to_wstring(items.size()) + L" 项？").c_str(), MB_OKCANCEL | MB_ICONQUESTION, title) != IDOK)
            return 0;

        // 计算哪些模块已经启用，已经启用的模块不能被删除
        std::wstring modules_enabled;

        for (auto i = items.begin(); i != items.end();) {
            auto mod = _modules[*i];

            if (mod->enable) {
                wchar_t guid[128];

                if (::StringFromGUID2(mod->guid, &guid[0], _countof(guid))) {
                    modules_enabled += L"    " + mod->name + L"\n";
                }
                i = items.erase(i);
            }
            else {
                ++i;
            }
        }

        if(!modules_enabled.empty()) {
            std::wstring msg;

            msg += L"以下模块由于已经启用而不能删除：\n\n";
            msg += modules_enabled;
            msg += L"\n是否仅删除被禁用的模块？";

            if (msgbox(msg, MB_ICONQUESTION | MB_OKCANCEL) == IDCANCEL)
                return 0;
        }

        _delete_items(items);

        return 0;
    }

    return 0;
}

// state == -1 仅对items为1个时有效
void ModuleManager::_enable_items(const std::vector<int>& items, int state)
{
    bool enable = state == -1 ? !_modules[items[0]]->enable : state == 1;

    for (auto i : items) {
        auto& mod = _modules[i];

        std::wstring err;

        if (!_on_toggle(mod, enable, &err)) {
            msgbox(err, MB_ICONERROR, mod->name);
            continue;
        }

        mod->enable = enable;

        _listview->redraw_items(i, i);
    }

    _on_item_state_change();
}

void ModuleManager::_modify_item(int i)
{
    auto& mod = _modules[i];

    if (mod->enable) return;

    auto onok = [&](ModuleEntry* entry) {
        _listview->redraw_items(i, i);
    };

    auto onguid = [&](const GUID& guid, std::wstring* err) {
        if (_has_guid(guid) && !::IsEqualGUID(guid, mod->guid)) {
            *err = L"此 GUID 已经存在。";
            return false;
        }

        return true;
    };

    (new ModuleEntryEditor(mod, onok, onguid))->domodal(this);
}

void ModuleManager::_delete_items(const std::vector<int>& items)
{
    // 从后往前删
    for (auto it = items.crbegin(); it != items.crend(); it++) {
        _modules.erase(_modules.begin() + *it);
    }

    // just in case
    _listview->set_item_count((int)_modules.size(), 0);

    _listview->redraw_items(0, _listview->get_item_count());

    _on_item_state_change();
}

void ModuleManager::_add_item()
{
    auto onok = [&](ModuleEntry* entry) {
        _modules.push_back(entry);
        _listview->set_item_count((int)_modules.size(), LVSICF_NOINVALIDATEALL);

        int index = _listview->get_item_count() - 1;
        _listview->ensure_visible(index);   // 确保可见
        _listview->set_item_state(-1, LVIS_SELECTED, 0); //取消选中其它的
        _listview->set_item_state(index, LVIS_SELECTED, LVIS_SELECTED); //选中当前新增的
        async_call([&]() {_listview->focus(); }); // 之前有窗口处于处于正在关闭状态，所以异步调
    };

    auto onguid = [&](const GUID& guid, std::wstring* err) {
        if (_has_guid(guid)) {
            *err = L"此 GUID 已经存在。";
            return false;
        }

        return true;
    };

    (new ModuleEntryEditor(nullptr, onok, onguid))->domodal(this);
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

int ModuleManager::_get_enable_state_for_items(const std::vector<int>& items)
{
    bool state = _modules[items[0]]->enable;
    bool same = true;

    for (auto i : items) {
        if (_modules[i]->enable != state) {
            same = false;
            break;
        }
    }

    return same ? (state ? 1 : 0) : -1;
}

void ModuleManager::_on_item_state_change()
{
    std::vector<int> items;
    _listview->get_selected_items(&items);

    _btn_enable->set_enabled(!items.empty());
    _btn_modify->set_enabled(items.size()==1 && items[0] != -1 && !_modules[items[0]]->enable);
    _btn_delete->set_enabled(items.size() > 1 && _get_enable_state_for_items(items) != 1 || (!items.empty() && !_modules[items[0]]->enable));

    if (!items.empty()) {
        int state = _get_enable_state_for_items(items);
        _btn_enable->set_text(state != -1 ? (state == 1 ? L"禁用" : L"启用") : L"启/禁");
    }
}

}
