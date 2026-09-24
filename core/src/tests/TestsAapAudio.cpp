// MyPods
// License: GPL-3.0

#include "TestsAapAudio.h"
#include "device/capabilities/aap/AapAudioSwitchCapability.h"
#include "device/capabilities/aap/AapAudioEffectsCapabilities.h"
#include "audio/AudioEffects.h"
#include "sdk/aap/enums/AapModelIds.h"
#include "StringUtils.h"
#include "Logger.h"
#include <cmath>

using namespace MagicPodsCore;

TestsAapAudio::TestsAapAudio()
{
    const std::string target = "AA:BB:CC:DD:EE:FF";

    // Reference bytes transcribed from LibrePods AACPManager.kt (createHijackRequestPacket / createSmartRoutingShowUIPacket)
    Test("HijackRequest matches LibrePods", AapAudioSwitchCapability::HijackRequest(target) == StringUtils::HexStringToBytes(
        "040004001000ffeeddccbbaa620001e54a6c6f63616c73636f7265306446726561736f6e4848696a61636b763251617564696f526f7574696e6753636f7265312d015f617564696f526f7574696e675365744f776e657273686970546f46616c7365014b72656d6f746573636f7265a5"));
    Test("ShowNearbyUI matches LibrePods", AapAudioSwitchCapability::ShowNearbyUI(target) == StringUtils::HexStringToBytes(
        "040004001000ffeeddccbbaa7e0001e65b536d617274526f7574696e674b657953686f774e65617262795549014a6c6f63616c73636f7265312d0146726561736f6e4868696a61636b763251617564696f526f7574696e6753636f7265a25f617564696f526f7574696e675365744f776e657273686970546f46616c7365014b72656d6f746573636f7265a2"));

    auto info = AapAudioSwitchCapability::MediaInformation(target, "11:22:33:44:55:66", "Linux");
    Test("MediaInformation length field", info.size() == 14u + (info[12] | (info[13] << 8)));
    Test("MediaInformation btName encoded", std::string(info.begin(), info.end()).find("\x46" "btName" "\x45" "Linux") != std::string::npos);

    Test("Banner name: adapter name, fallback", AapAudioSwitchCapability::BannerName("Chris-Laptop") == "Chris-Laptop" &&
                                                     AapAudioSwitchCapability::BannerName("") == "Linux");
    // 31 ASCII bytes and a 2-byte "ä": the cut must not split the character
    auto banner = AapAudioSwitchCapability::BannerName(std::string(31, 'x') + "\xC3\xA4" + "yz");
    Test("Banner name: at most 32 bytes, whole characters", banner == std::string(31, 'x'));
    Test("OwnsConnection", AapAudioSwitchCapability::OwnsConnection(true) == StringUtils::HexStringToBytes("0400040009000601000000"));

    auto source = AapAudioSwitchCapability::ParseAudioSource(StringUtils::HexStringToBytes("040004000e0066554433221102"));
    Test("ParseAudioSource", source && source->first == "11:22:33:44:55:66" && source->second == 2);

    auto devices = AapAudioSwitchCapability::ParseConnectedDevices(StringUtils::HexStringToBytes("040004002e000000021122334455660101aabbccddeeff0202"));
    Test("ParseConnectedDevices", devices == std::vector<std::string>{"11:22:33:44:55:66", "AA:BB:CC:DD:EE:FF"});

    // Head tracking: o2 at 45, o3 at 47 (int16 LE); turning moves them in opposite directions
    std::vector<unsigned char> packet(70, 0);
    packet[45] = 0xE8; packet[46] = 0x03; // o2 = 1000
    packet[47] = 0x18; packet[48] = 0xFC; // o3 = -1000
    Test("No head tracking on AirPods 2", !AapSpatialAudioCapability::HasHeadTracking(static_cast<unsigned short>(AapModelIds::airpods2)));
    Test("Head tracking on AirPods Pro 3 and Max 2", AapSpatialAudioCapability::HasHeadTracking(static_cast<unsigned short>(AapModelIds::airpodspro3)) &&
                                                     AapSpatialAudioCapability::HasHeadTracking(static_cast<unsigned short>(AapModelIds::airpodsmax2)));
    Test("Head tracking yaw", std::abs(AapSpatialAudioCapability::Yaw(packet, 0, 0) - 5.625) < 1e-9);

    EffectsConfig config;
    Test("Neutral config means no chain", config.IsNeutral());
    config.spatial = SpatialMode::Fixed;
    config.eq = *AudioEffects::Preset("Bass Booster");
    auto conf = AudioEffects::BuildConfig("bluez_output.AA_BB.1", "Air\"Pods", config, 0);
    Test("Chain: EQ feeds spatializer, mixers are the outputs",
         conf.find("inputs = [ \"preL:In\" \"preR:In\" ]") != std::string::npos &&
         conf.find("{ output = \"eqL9:Out\" input = \"spL:In\" }") != std::string::npos &&
         conf.find("outputs = [ \"mixL:Out\" \"mixR:Out\" ]") != std::string::npos &&
         conf.find("target.object = \"bluez_output.AA_BB.1\"") != std::string::npos);
    Test("Chain: boost is compensated by pre-gain", conf.find("Gain = -5.50") != std::string::npos);
    Test("Chain: EQ gains are live controls", AudioEffects::ControlCommand(config, 0).find("\"eqR0:Gain\" 5.50") != std::string::npos &&
                                              AudioEffects::BuildConfig("s", "d", EffectsConfig{SpatialMode::Fixed, {}}, 0).find("name = eqL0") != std::string::npos);
    Test("Chain: device name cannot break the config", conf.find("AirPods") != std::string::npos && conf.find("Air\"Pods") == std::string::npos);
}

void TestsAapAudio::Test(const char *name, bool ok)
{
    if (!ok)
        failures++;
    Logger::Info("%-50s %s", name, ok ? "OK" : "FAILED");
}
