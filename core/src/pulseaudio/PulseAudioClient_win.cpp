// MyPods
// License: GPL-3.0

// Windows: nothing to talk to (see PulseAudioClient.h). Every query answers "not there", so the codec
// capability stays hidden and routing is left to Windows, which switches to connected headphones itself.

#include "PulseAudioClient.h"

namespace MagicPodsCore
{
    PulseAudioClient::PulseAudioClient() = default;
    PulseAudioClient::~PulseAudioClient() = default;

    bool PulseAudioClient::SetCardProfile(const std::string &, const std::string &) { return false; }
    std::optional<CardInfo> PulseAudioClient::GetCardInfoByName(const std::string &) { return std::nullopt; }
    std::string PulseAudioClient::GetNameFromMac(const std::string &mac) { return mac; }
    std::optional<std::string> PulseAudioClient::FindSink(const std::string &) { return std::nullopt; }
    bool PulseAudioClient::SetDefaultSink(const std::string &) { return false; }
}
