#pragma once

namespace taolog {

class EventContainer
{
protected:
    typedef LogDataUIPtr EVENT;
    typedef std::function<bool(const EVENT event)> FILTER;
    typedef std::vector<EVENT> EVENTS;

public:
    EventContainer()
    {
        is_tmp = false;
        enable_filter(false);
    }

    EventContainer(
        const std::wstring& name,
        int field_index, const std::wstring& field_name,
        int value_index, const std::wstring& value_name,
        const std::wstring& value_input,
        bool enable
        )
        : name(name)
        , field_index(field_index)
        , field_name(field_name)
        , value_index(value_index)
        , value_name(value_name)
        , value_input(value_input)
    {
        enable_filter(enable);
    }

public:
    json11::Json to_json() const;
    std::wstring to_tip() const;
    static EventContainer* from_json(const json11::Json& obj) throw(...);

public:
    bool add(EVENT& evt);
    bool filter_results(EventContainer* container);
    EVENT operator[](int index) { return _events[index]; }
    size_t size() const { return _events.size(); }
    void clear() { return _events.clear(); }
    EVENTS& events() { return _events; }
    void enable_filter(bool b);
    bool is_lua() const { return !value_input.empty() && value_input[0] == '`'; }

public:
    std::wstring name;              // 容器的名字
    std::wstring field_name;        // 基于哪个字段
    int          field_index;       // 字段索引
    std::wstring value_name;        // 所选的值的名字
    int          value_index;       // 所选的值的索引
    std::wstring value_input;       // 自定义输入

    bool         is_tmp;            // 临时使用的

    lua_State*   lua;               // LUA
    int          lua_cookie;

protected:

    EVENTS          _events;
    FILTER          _filter;

    std::wstring _value_input_lower;
    std::wregex  _regex_input;
};

typedef std::vector<EventContainer*> EventContainerS;

typedef std::pair<EventContainer, EventContainerS> EventPair;

class EventSearcher
{
public:
    EventSearcher()
        : _last(-1)
        , _valid(false)
    { }

public:
    void reset(EventContainer* haystack, const std::vector<int>& cols, const std::wstring& s) throw(...);
    int  next(bool forward = true);
    int last(int i);
    int  last() const;
    void invalid();
    const std::wstring& s() const;
    const bool (&match_cols() const)[LogDataUI::cols()];

protected:
    bool                _valid;
    std::vector<int>    _cols;
    bool                _match_cols[LogDataUI::cols()];
    int                 _last;
    EventContainer*     _haystack;
    EventContainer      _needle;
};

} // namespace taolog
