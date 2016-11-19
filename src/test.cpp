#include <iostream>

#include <windows.h>
#include <guiddef.h>

#define ETW_LOGGER

#include "etwlogger.h"

// {630514B5-7B96-4B74-9DB6-66BD621F9386}
static const GUID providerGuid = 
{ 0x630514b5, 0x7b96, 0x4b74, { 0x9d, 0xb6, 0x66, 0xbd, 0x62, 0x1f, 0x93, 0x86 } };

ETWLogger g_etwLogger(providerGuid);


int main()
{
    int i = 0;

start:

    EtwVbs(_T("Verbose\n123333333333333333333333333333333\n555555555555555\nafasdfasfasfd\n\n4028402394"));
    EtwLog(_T("Information"));
    EtwWrn(_T("Warning"));
    EtwErr(_T("Error"));
    EtwFat(_T("Critical"));
    EtwVbs(_T("Longstring jasl;alkdjfklasfjdklajsfeaskdlfjsakdfjasklfj asalalfas asdfaslfk"));
    EtwLog(_T("11111111111111111111111111111111111111111111111111111112222222222222222222222222222222222222222222222333333333333333333333333333333333333333333333444444444444444444444444444444444444444444444455555555555555555555555555555555555555555"));
    EtwLog(_T("%s"), _T(R"!!!(LRESULT MainWindow::_on_log(LoggerMessage::Value msg, LPARAM lParam)
{
    if(msg == LoggerMessage::AllocMsg) {
        return (LRESULT)_log_pool.alloc();
    }
    else if(msg == LoggerMessage::DeallocMsg) {
        auto p = reinterpret_cast<LogDataUI*>(lParam);
        p->~LogDataUI();
        _log_pool.destroy(p);
        return 0;
    }
    else if(msg == LoggerMessage::LogMsg) {
        LogDataUIPtr item((LogDataUI*)lParam, [&](LogDataUI* p) {
            _on_log(LoggerMessage::DeallocMsg, LPARAM(p));
        });

        const std::wstring* root = nullptr;

        _snwprintf(item->id, _countof(item->id), L"%llu", (unsigned long long)_events.size() + 1);

        _module_from_guid(item->guid, &item->strProject, &root);

        // 相对路径
        item->offset_of_file = 0;
        if(*item->file && root) {
            if(::_wcsnicmp(item->file, root->c_str(), root->size()) == 0) {
                item->offset_of_file = (int)root->size();
            }
        }

        item->strLevel = &_level_maps[item->level].cmt1;

        // 全部事件容器
        _events.add(item);

        // 带过滤的事件容器（指针复用）
        if(!_filters.empty()) {
            for(auto& f : _filters) {
                f->add(item);
            }
        }

        // 默认是非自动滚屏到最后一行的
        // 但如果当前焦点行是最后一行，则自动滚屏
        int count = (int)_current_filter->size();
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

        if(_miniview) {
            _miniview->update();
        }
    }

    return 0;
}
)!!!"));

    OutputDebugStringA("OutputDebugStringA\n");



    printf("(%d) %s", ++i, "After OutputDebugStringA\n");

    Sleep(3000);

    goto start;

    return 0;
}
