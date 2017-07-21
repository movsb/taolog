#pragma once

namespace taolog {

struct Column {
	std::string id;		// 用于内部识别列
    std::wstring name;
    bool show;
    int width;
    bool valid;         // 临时使用
    int index;

    Column(const wchar_t* name_, bool show_, int width_, const char* id_)
        : name(name_)
        , show(show_)
        , width(width_)
		, id(id_)
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
        };
    }
};

typedef std::function<void(int i)> OnToggleCallback;

class ColumnManager
{
public:
    struct ColumnFlags
    {
        enum Value
        {
            All,
            Available,
            Showing,
        };
    };

    typedef std::vector<Column> ColumnContainer;
    typedef std::vector<int> TypeIndices;

public:
    template<typename ...T>
    void push(T... args) {
        _columns.emplace_back(std::forward<T>(args)...);
        _columns.back().index = (int)_columns.size()-1;
    }
    Column& operator[](size_t i) { return _columns[i]; }
    size_t size() const { return _columns.size(); }

    void update();
    void for_each(ColumnFlags::Value f, std::function<void(int i, Column& c)> fn);
    void show(int available_index, int* listview_index);
    void hide(bool is_listview_index, int index, int* listview_delete_index);

    bool any_showing() const {return !_showing_indices.empty(); }
    Column& showing(int listview_index) { return _columns[_showing_indices[listview_index]]; }
    Column& avail(int index) { return _columns[_available_indices[index]]; }

    ColumnContainer& all() { return _columns; }

protected:
    ColumnContainer _columns;
    TypeIndices     _available_indices;
    TypeIndices     _showing_indices;
};

class ColumnSelection : public taowin::WindowCreator
{
public:
    ColumnSelection(ColumnManager& colmgr)
        : _columns(colmgr)
    { }

    void OnToggle(OnToggleCallback fn) { _on_toggle = fn; }

protected:
    virtual void get_metas(WindowMeta* metas) override;
    virtual LPCTSTR get_skin_xml() const override;
    virtual LRESULT handle_message(UINT umsg, WPARAM wparam, LPARAM lparam) override;
    virtual LRESULT on_notify(HWND hwnd, taowin::Control* pc, int code, NMHDR* hdr);
    virtual void on_final_message() override { __super::on_final_message(); delete this; }

protected:
    OnToggleCallback    _on_toggle;
    ColumnManager&      _columns;
};

} // namespace taolog
