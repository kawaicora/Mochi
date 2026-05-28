#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <cstdint>
class EventToken {
public:
    uint64_t id = 0;

    bool IsValid() const { return id != 0; }
};

template<typename... Args>
class Event {
public:
    using Handler = std::function<void(Args...)>;

    EventToken Subscribe(Handler handler) {
        EventToken token;
        token.id = ++_nextId;
        _handlers[token.id] = std::move(handler);
        return token;
    }

    void Unsubscribe(EventToken token) {
        if (!token.IsValid()) return;
        _handlers.erase(token.id);
    }

    void Clear() {
        _handlers.clear();
    }

    void Invoke(Args... args) {
        // 防止回调里修改容器导致崩溃
        auto handlersCopy = _handlers;

        for (auto& [id, handler] : handlersCopy) {
            if (handler) {
                handler(args...);
            }
        }
    }

    bool Empty() const {
        return _handlers.empty();
    }

private:
    std::unordered_map<uint64_t, Handler> _handlers;
    uint64_t _nextId = 0;
};

