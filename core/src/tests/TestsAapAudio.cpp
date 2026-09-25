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
    Test("Chain: EQ feeds HRTF compensation and spatializer, mixers are the outputs",
         conf.find("inputs = [ \"preL:In\" \"preR:In\" ]") != std::string::npos &&
         conf.find("{ output = \"eqL9:Out\" input = \"scL0:In\" }") != std::string::npos &&
         conf.find("{ output = \"scL9:Out\" input = \"spL:In\" }") != std::string::npos &&
         conf.find("{ output = \"lpR1:Out\" input = \"mixL:In 4\" }") != std::string::npos && // bass bypasses the HRTF
         conf.find("outputs = [ \"mixL:Out\" \"mixR:Out\" ]") != std::string::npos &&
         conf.find("target.object = \"bluez_output.AA_BB.1\"") != std::string::npos);
    // Bass Booster's bands add up to +6.48 dB (largest band 5.5), the spatial path adds 10.4 dB
    Test("Chain: summed boost and spatial peak are taken off by the pre-gain", conf.find("name = preL control = { Freq = 0.00 Q = 1.00 Gain = -16.") != std::string::npos);
    auto custom = config;
    custom.sofa = "/home/me/personal.sofa";
    auto customConf = AudioEffects::BuildConfig("s", "d", custom, 0);
    Test("Chain: custom SOFA is used without the KEMAR fixes", customConf.find("filename = \"/home/me/personal.sofa\"") != std::string::npos &&
                                                                      customConf.find("scL0") == std::string::npos &&
                                                                      customConf.find("lpL0") == std::string::npos);

    // adjacent peaking bands overlap: the old "largest band" pre-gain left Dance and R&B clipping by 1.7 dB
    auto headroom = [](const char *preset) { EffectsConfig c; c.eq = *AudioEffects::Preset(preset); return AudioEffects::HeadroomDb(c); };
    Test("Headroom: summed EQ curve, not the largest band", std::abs(headroom("Dance") - 8.24) < 0.1 && std::abs(headroom("R&B") - 8.58) < 0.1 &&
                                                            headroom("Off") == 0 && headroom("Bass Reducer") == 0);

    EffectsConfig crossfeed;
    crossfeed.crossfeed = true;
    // L' = L + g * lowpass(R - L): only signals that differ between the channels can exceed full scale, by 1.6 dB at most
    Test("Crossfeed: not neutral, worst case +1.6 dB", !crossfeed.IsNeutral() && std::abs(AudioEffects::HeadroomDb(crossfeed) - 1.63) < 0.1);
    auto xconf = AudioEffects::BuildConfig("s", "d", crossfeed, 0);
    Test("Crossfeed: each ear adds the other's low-pass and takes off its own",
         xconf.find("{ output = \"eqR9:Out\" input = \"xlR:In\" }") != std::string::npos &&
         xconf.find("{ output = \"xlR:Out\" input = \"xmL:In 3\" }") != std::string::npos &&
         xconf.find("{ output = \"xlL:Out\" input = \"xmL:In 2\" }") != std::string::npos &&
         xconf.find("outputs = [ \"xmL:Out\" \"xmR:Out\" ]") != std::string::npos &&
         AudioEffects::ControlCommand(crossfeed, 0).find("\"xmL:Gain 2\" -0.37 \"xmL:Gain 3\" 0.37") != std::string::npos);
    crossfeed.crossfeed = false;
    Test("Crossfeed off: nodes stay, low-passed paths muted", AudioEffects::ControlCommand(crossfeed, 0).find("\"xmL:Gain 2\" 0.00 \"xmL:Gain 3\" 0.00") != std::string::npos &&
                                                             AudioEffects::BuildConfig("s", "d", crossfeed, 0).find("\"Gain 2\" = 0.00 \"Gain 3\" = 0.00") != std::string::npos);

    EffectsConfig corrected;
    corrected.correction = AapEqualizerCapability::Correction(static_cast<unsigned short>(AapModelIds::airpodsmax));
    Test("Correction: AirPods Max measured, USB-C shares it, Pro 3 has none", corrected.correction.size() == 10 &&
         AapEqualizerCapability::Correction(static_cast<unsigned short>(AapModelIds::airpodsmax2024)) == corrected.correction &&
         AapEqualizerCapability::Correction(static_cast<unsigned short>(AapModelIds::airpodspro3)).empty());
    Test("Correction off: neutral, nodes at 0 dB", corrected.IsNeutral() && AudioEffects::ControlCommand(corrected, 0).find("\"coL0:Gain\" 0.00") != std::string::npos);
    corrected.corrected = true;
    Test("Correction on: filters after the EQ with their gains", !corrected.IsNeutral() &&
         AudioEffects::BuildConfig("s", "d", corrected, 0).find("label = bq_lowshelf name = coL0 control = { Freq = 105.00 Q = 0.70 Gain = -3.00 }") != std::string::npos &&
         AudioEffects::ControlCommand(corrected, 0).find("\"coR5:Gain\" -5.50") != std::string::npos);
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
