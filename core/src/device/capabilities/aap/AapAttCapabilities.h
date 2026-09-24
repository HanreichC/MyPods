// MyPods
// License: GPL-3.0

#pragma once
#include "AapCapability.h"
#include "sdk/aap/Att.h"
#include <mutex>

namespace MagicPodsCore
{
    // Settings AirPods Pro 2/3 keep as GATT characteristics (AapDevice::AttRead/AttWrite). Shown once
    // their value has been read, so they stay hidden when the ATT channel can't be opened.
    class AapAttCapability : public AapCapability
    {
    private:
        size_t attEventId = 0;

    protected:
        unsigned char handle;
        virtual void OnValue(const std::vector<unsigned char> &value) = 0;
        void OnReceivedData(const std::vector<unsigned char> &) override {}

    public:
        AapAttCapability(const std::string &name, unsigned char handle, AapDevice &device);
        ~AapAttCapability() override;
    };

    // Loud Sound Reduction: lowers loud surroundings in transparency and adaptive mode
    class AapLoudSoundReductionCapability : public AapAttCapability
    {
    private:
        std::atomic<bool> enabled{false};

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnValue(const std::vector<unsigned char> &value) override;

    public:
        explicit AapLoudSoundReductionCapability(AapDevice &device) : AapAttCapability("loudSoundReduction", Att::LOUD_SOUND_REDUCTION, device) {}
        void SetFromJson(const nlohmann::json &json) override;
    };

    // Customized transparency mode. JSON is what the iPhone offers: amplification and balance (-1..1),
    // tone (-1..1), ambient noise reduction (0..1), conversation boost. The per-band EQ stays as read.
    // ponytail: ranges follow LibrePods, not measured on hardware; the raw floats are clamped, not scaled
    class AapTransparencyCapability : public AapAttCapability
    {
    private:
        std::mutex lock;
        Att::TransparencySettings settings{};

    protected:
        nlohmann::json CreateJsonBody() override;
        void OnValue(const std::vector<unsigned char> &value) override;

    public:
        explicit AapTransparencyCapability(AapDevice &device) : AapAttCapability("transparencyTuning", Att::TRANSPARENCY, device) {}
        void SetFromJson(const nlohmann::json &json) override;

        static nlohmann::json ToJson(const Att::TransparencySettings &settings);
        // Applies the fields present in `json`, the others keep their value
        static void Apply(Att::TransparencySettings &settings, const nlohmann::json &json);
    };
}
