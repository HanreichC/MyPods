// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace MagicPodsCore {

    // Subscribed and fired from several threads (D-Bus, AAP reader, MPRIS, PulseAudio, WebSocket loop).
    // FireEvent calls a snapshot, so listeners may (un)subscribe while it runs. Once Unsubscribe returns, the
    // listener is never called again and a call on another thread has finished: an owner that unsubscribes in
    // its destructor can't be called after it is gone. A listener may unsubscribe itself during its own call.
    // Unsubscribe must not hold a lock the listener takes (it waits for that call to end).
    template<typename DataType>
    class Event {
    private:
        struct Listener {
            std::function<void(size_t, const DataType&)> function;
            std::recursive_mutex calling{}; // held for each call; recursive, so a listener can unsubscribe itself
            bool alive = true;              // under `calling`
        };

        std::mutex _lock{};
        size_t _idForNewListener{};
        std::map<size_t, std::shared_ptr<Listener>> _listeners{};

    public:
        size_t Subscribe(std::function<void(size_t, const DataType&)>&& listener);
        void Unsubscribe(size_t listenerId);
        void FireEvent(const DataType& dataType);
    };

    template<typename DataType>
    size_t Event<DataType>::Subscribe(std::function<void(size_t, const DataType&)>&& listener) {
        auto entry = std::make_shared<Listener>();
        entry->function = std::move(listener);
        std::lock_guard lock{_lock};
        _listeners.emplace(++_idForNewListener, std::move(entry));
        return _idForNewListener;
    }

    template<typename DataType>
    void Event<DataType>::Unsubscribe(size_t listenerId) {
        std::shared_ptr<Listener> entry;
        {
            std::lock_guard lock{_lock};
            auto it = _listeners.find(listenerId);
            if (it == _listeners.end())
                return;
            entry = std::move(it->second);
            _listeners.erase(it);
        }
        std::lock_guard calling{entry->calling}; // a call on another thread finishes first
        entry->alive = false;
    }

    template<typename DataType>
    void Event<DataType>::FireEvent(const DataType& dataType) {
        std::map<size_t, std::shared_ptr<Listener>> listeners;
        {
            std::lock_guard lock{_lock};
            listeners = _listeners;
        }
        for (auto& [key, entry] : listeners) {
            std::lock_guard calling{entry->calling};
            if (entry->alive)
                entry->function(key, dataType);
        }
    }
}
