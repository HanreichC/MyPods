// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include <functional>
#include <map>
#include <mutex>

namespace MagicPodsCore {

    // Subscribed and fired from several threads (D-Bus, AAP reader, MPRIS, PulseAudio, WebSocket loop).
    // FireEvent calls a snapshot, so listeners may (un)subscribe while it runs.
    // ponytail: a listener unsubscribed by another thread mid-fire can still get that one call;
    // owners that go away join their threads first (Device::Shutdown).
    template<typename DataType>
    class Event {
    private:
        std::mutex _lock{};
        size_t _idForNewListener{};
        std::map<size_t, std::function<void(size_t, const DataType&)>> _listeners{};

    public:
        size_t Subscribe(std::function<void(size_t, const DataType&)>&& listener);
        void Unsubscribe(size_t listenerId);
        void FireEvent(const DataType& dataType);
    };

    template<typename DataType>
    size_t Event<DataType>::Subscribe(std::function<void(size_t, const DataType&)>&& listener) {
        std::lock_guard lock{_lock};
        _listeners.emplace(++_idForNewListener, std::move(listener));
        return _idForNewListener;
    }

    template<typename DataType>
    void Event<DataType>::Unsubscribe(size_t listenerId) {
        std::lock_guard lock{_lock};
        _listeners.erase(listenerId);
    }

    template<typename DataType>
    void Event<DataType>::FireEvent(const DataType& dataType) {
        std::map<size_t, std::function<void(size_t, const DataType&)>> listeners;
        {
            std::lock_guard lock{_lock};
            listeners = _listeners;
        }
        for (auto& [key, value] : listeners)
            value(key, dataType);
    }
}
