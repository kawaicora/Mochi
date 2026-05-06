#pragma once
#include <coroutine>
#include <vector>
#include <algorithm>

struct Coroutine {
    struct promise_type;
    using handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        bool stopped = false;

        Coroutine get_return_object() {
            return Coroutine{ handle::from_promise(*this) };
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}
        void unhandled_exception() {}

        void request_stop() { stopped = true; }
        bool is_stopped() const { return stopped; }
    };

    handle h{};

    Coroutine(handle h) : h(h) {}
    Coroutine(const Coroutine&) = delete;
    Coroutine(Coroutine&& other) noexcept : h(other.h) {
        other.h = nullptr;
    }

    ~Coroutine() {
        if (h) h.destroy();
    }

    void resume() {
        if (!h || h.done()) return;
        if (h.promise().is_stopped()) return;

        h.resume();
    }

    void stop() {
        if (h) {
            h.promise().request_stop();
        }
    }

    bool done() const {
        return !h || h.done();
    }
};


struct CoroutineManager {
    static std::vector<Coroutine> coroutines;

    static void Add(Coroutine&& co) {
        coroutines.emplace_back(std::move(co));
    }

    static void Update() {
        for (auto& co : coroutines) {
            co.resume();
        }

        coroutines.erase(
            std::remove_if(coroutines.begin(), coroutines.end(),
                [](const Coroutine& co) {
                    return co.done();
                }),
            coroutines.end()
        );
    }

    static void StopAll() {
        for (auto& co : coroutines) {
            co.stop();
        }
    }
};

