// MyPods
// License: GPL-3.0

#include "BeoplayE8.h"

namespace MyPods::BeoplayE8
{
    bool Is(const std::string &name)
    {
        // the 2nd and 3rd generation call themselves "Beoplay E8 2.0" / "E8 3.0" and speak something else
        return name == "Beoplay E8";
    }

    std::optional<bool> Transparency(const std::vector<unsigned char> &state)
    {
        if (state.size() != 3 || state[0] != 0x03)
            return std::nullopt;
        if (state[2] == 0x02)
            return false;
        if (state[2] == 0x03)
            return true;
        return std::nullopt;
    }

#ifndef _WIN32
    // ponytail: BlueZ only offers GATT on an LE link, which it doesn't open for headphones connected over BR/EDR;
    // until it does (Device1.Connect with an LE bearer), the E8 is plain hands-free headphones on Linux
    struct Link::Native {};
    Link::Link(const std::string &, std::function<void(const Info &)>, std::function<void(bool)>) {}
    Link::~Link() = default;
#endif
}
