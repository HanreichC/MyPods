// MyPods
// License: GPL-3.0

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MagicPodsCore
{
    // Bang & Olufsen Beoplay E8 (1st gen, 2017), next to its audio, over Bluetooth LE (docs/beoplay-e8.md): the
    // standard Device Information service, and in B&O's service 0xFE89 the state characteristic 4446cf5f…, whose
    // notifications are [03, paused, mode] with mode 02 normal and 03 transparency. A single byte written there is a
    // remote command (02 previous, 03 next), not a mode: how the app sets transparency is not known yet, so this only
    // reads.
    namespace BeoplayE8
    {
        struct Info
        {
            std::string model, serial, firmware;
        };

        bool Is(const std::string &name);
        // transparency on or off from a state notification, nullopt for anything else
        std::optional<bool> Transparency(const std::vector<unsigned char> &state);

        // While it lives: connects over LE (again, after the link drops) and reports the device info once a
        // connection and the transparency state on every change. Windows (WinRT); elsewhere it does nothing yet.
        class Link
        {
        public:
            Link(const std::string &address, std::function<void(const Info &)> onInfo, std::function<void(bool)> onTransparency);
            ~Link();

        private:
            struct Native;
            std::unique_ptr<Native> native;
        };
    }
}
