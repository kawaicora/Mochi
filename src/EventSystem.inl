#pragma once

template<typename... Args>
inline EventToken Event<Args...>::Subscribe(Handler handler) {
    EventToken token;
    token.id = ++_nextId;

    _handlers[token.id] = std::move(handler);

    return token;
}
template<typename... Args>
inline void Event<Args...>::Unsubscribe(EventToken token) {
    if (!token.IsValid())
        return;

    _handlers.erase(token.id);
}
template<typename... Args>
inline void Event<Args...>::Clear() {
    _handlers.clear();
}
template<typename... Args>
inline void Event<Args...>::Invoke(Args... args) {
    // 防止回调中修改事件列表
    auto handlersCopy = _handlers;

    for (auto& [id, handler] : handlersCopy) {
        if (handler) {
            handler(args...);
        }
    }
}
template<typename... Args>
inline bool Event<Args...>::Empty() const {
    return _handlers.empty();
}