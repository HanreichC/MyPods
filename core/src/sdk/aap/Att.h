// MyPods
// License: GPL-3.0

#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <vector>

namespace MagicPodsCore::Att
{
    // AirPods Pro 2/3 keep some settings as GATT characteristics, reached over an ATT channel on
    // L2CAP PSM 0x1F next to AAP. Handles and layouts: LibrePods (ATTManager.kt, Transparency.kt).
    constexpr unsigned short PSM = 0x1F;

    constexpr uint8_t ERROR_RSP = 0x01, READ_REQ = 0x0A, READ_RSP = 0x0B, WRITE_REQ = 0x12, WRITE_RSP = 0x13, NOTIFY = 0x1B;

    constexpr uint8_t TRANSPARENCY = 0x18;         // customized transparency mode
    constexpr uint8_t LOUD_SOUND_REDUCTION = 0x1B; // one byte, 1 on / 0 off

    inline std::vector<uint8_t> Read(uint8_t handle)
    {
        return {READ_REQ, handle, 0x00};
    }

    inline std::vector<uint8_t> Write(uint8_t handle, const std::vector<uint8_t> &value)
    {
        std::vector<uint8_t> pdu{WRITE_REQ, handle, 0x00};
        pdu.insert(pdu.end(), value.begin(), value.end());
        return pdu;
    }

    // Transparency characteristic: little-endian floats. enabled, then per bud (left, right):
    // 8 EQ bands, amplification, tone, conversation boost (0/1), ambient noise reduction; newer
    // firmware appends own voice amplification.
    struct TransparencySettings
    {
        struct Bud
        {
            std::array<float, 8> eq{};
            float amplification = 0, tone = 0, conversationBoost = 0, ambientNoiseReduction = 0;
        };
        bool enabled = false;
        Bud left{}, right{};
        std::optional<float> ownVoice{};

        static std::optional<TransparencySettings> Parse(const std::vector<uint8_t> &data)
        {
            if (data.size() < 100)
                return std::nullopt;
            size_t at = 0;
            auto next = [&]() {
                uint32_t bits = data[at] | (data[at + 1] << 8) | (data[at + 2] << 16) | (static_cast<uint32_t>(data[at + 3]) << 24);
                at += 4;
                return std::bit_cast<float>(bits);
            };
            TransparencySettings s;
            s.enabled = next() > 0.5f;
            for (Bud *bud : {&s.left, &s.right})
            {
                for (auto &band : bud->eq)
                    band = next();
                bud->amplification = next();
                bud->tone = next();
                bud->conversationBoost = next();
                bud->ambientNoiseReduction = next();
            }
            if (data.size() >= 104)
                s.ownVoice = next();
            return s;
        }

        std::vector<uint8_t> Encode() const
        {
            std::vector<uint8_t> out;
            auto put = [&](float value) {
                uint32_t bits = std::bit_cast<uint32_t>(value);
                for (int i = 0; i < 4; i++)
                    out.push_back(static_cast<uint8_t>(bits >> (8 * i)));
            };
            put(enabled ? 1.0f : 0.0f);
            for (const Bud *bud : {&left, &right})
            {
                for (float band : bud->eq)
                    put(band);
                put(bud->amplification);
                put(bud->tone);
                put(bud->conversationBoost);
                put(bud->ambientNoiseReduction);
            }
            if (ownVoice)
                put(*ownVoice);
            return out;
        }
    };
}
