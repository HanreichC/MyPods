// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "sdk/sgb/enums/GalaxyBudsModelIds.h"
#include "sdk/sgb/structs/GalaxyBudsResponseData.h"
#include "Crc16.h"
#include <vector>
#include <optional>
#include <iostream>

namespace MagicPodsCore
{

    class GalaxyBudsPacket
    {
    private:
        bool isLegacy = false;
        GalaxyBudsModelIds model;

    public:
        unsigned char startOfMessage = 0;
        unsigned char endOfMessage = 0;
        explicit GalaxyBudsPacket(GalaxyBudsModelIds model);
        // Takes the complete packets off the front of `pending` (bytes as they came off the stream),
        // leaves an incomplete tail and drops bytes that can't start a packet
        std::vector<std::vector<unsigned char>> Split(std::vector<unsigned char> &pending) const;
        // One complete packet, as Split returns it
        std::optional<GalaxyBudsResponseData> Extract(const std::vector<unsigned char> &buffer);
        std::vector<unsigned char> Encode(GalaxyBudsMsgIds id, const std::vector<unsigned char> &payload);
    };
}