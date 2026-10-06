// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "Event.h"

#include <mutex>

namespace MagicPodsCore {

    // Set on a D-Bus (or WinRT) thread, read from any other: values are handed out as copies
    template<typename DataType>
    class ObservableVariable {
    private:
        Event<DataType> _event{};
        mutable std::mutex _lock{};
        DataType _value{};

    public:
        explicit ObservableVariable(DataType initialValue) : _value{initialValue} {
        }

        Event<DataType>& GetEvent() {
            return _event;
        }

        DataType GetValue() const {
            std::lock_guard lock{_lock};
            return _value;
        }

        void SetValue(const DataType& newValue) {
            {
                std::lock_guard lock{_lock};
                _value = newValue;
            }
            _event.FireEvent(newValue);
        }
    };

}