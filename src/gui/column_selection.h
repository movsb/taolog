#pragma once

namespace taoetw {

struct Column {
	std::string id;		// 用于内部识别列
    int log_index;      // 对应日志的哪个字段
    std::wstring name;
    bool show;
    int width;
    bool valid;         // 临时使用

    Column(const wchar_t* name_, bool show_, int width_, const char* id_, int log_index_)
        : name(name_)
        , show(show_)
        , width(width_)
		, id(id_)
        , log_index(log_index_)
    {
        valid = true;
    }

    json11::Json to_json() const
    {
        return json11::Json::object {
            {"name", g_config.us(name)},
            {"show", show},
            {"width", width},
			{"id", id},
            {"li", log_index},
        };
    }
};

typedef std::function<void(int i)> OnToggleCallback;
typedef std::vector<Column> ColumnContainer;

class ColumnSelection : public taowin::window_creator
{
public:
    ColumnSelection(ColumnContainer& cols)
        : _columns(cols)
    { }

    void OnToggle(OnToggleCallback fn) { _on_toggle = fn; }

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::control* pc, int code, NMHDR* hdr);
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

protected:
    OnToggleCallback    _on_toggle;
    ColumnContainer&    _columns;
};

} // namespace taoetw
