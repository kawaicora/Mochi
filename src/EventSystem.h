#pragma once
#include <functional>
#include <unordered_map>
#include <cstdint>

class EventToken {
public:
    uint64_t id = 0;
    bool IsValid() const;
};

template<typename... Args>
class Event {
public:
    using Handler = std::function<void(Args...)>;
    EventToken Subscribe(Handler handler);
    void Unsubscribe(EventToken token);
    void Clear();
    void Invoke(Args... args);
    bool Empty() const;

private:
    std::unordered_map<uint64_t, Handler> _handlers;
    uint64_t _nextId = 0;
};


#include "EventSystem.inl"