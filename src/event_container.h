#pragma once

namespace taoetw {

class EventContainer
{
protected:
    typedef LogDataUIPtr EVENT;
    typedef std::function<bool(const EVENT event)> FILTER;
    typedef std::vector<EVENT> EVENTS;

public:
    EventContainer()
    {}

    EventContainer(
        const std::wstring& name,
        int field_index, const std::wstring& field_name,
        int value_index, const std::wstring& value_name,
        const std::wstring& value_input
        )
        : name(name)
        , field_index(field_index)
        , field_name(field_name)
        , value_index(value_index)
        , value_name(value_name)
        , value_input(value_input)
    {
        _init();
    }

public:
    json11::Json to_json() const;
    static EventContainer* from_json(const json11::Json& obj);

public:
    bool add(EVENT& evt);
    bool filter_results(EventContainer* container);
    EVENT operator[](int index) { return _events[index]; }
    size_t size() const { return _events.size(); }
    void clear() { return _events.clear(); }
    EVENTS& events() { return _events; }

protected:
    void _init();

public:
    std::wstring name;              // 容器的名字
    std::wstring field_name;        // 基于哪个字段
    int          field_index;       // 字段索引
    std::wstring value_name;        // 所选的值的名字
    int          value_index;       // 所选的值的索引
    std::wstring value_input;       // 自定义输入

protected:

    EVENTS          _events;
    FILTER          _filter;
};

typedef std::vector<EventContainer*> EventContainerS;

}
