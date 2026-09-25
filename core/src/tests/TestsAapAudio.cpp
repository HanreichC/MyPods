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
#include <cstdio>

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
    Test("Chain: HRTF compensation and spatializer, then the filters per ear",
         conf.find("inputs = [ \"preL:In\" \"preR:In\" ]") != std::string::npos &&
         conf.find("{ output = \"preL:Out\" input = \"scL0:In\" }") != std::string::npos &&
         conf.find("{ output = \"scL9:Out\" input = \"spL:In\" }") != std::string::npos &&
         conf.find("{ output = \"preR:Out\" input = \"bass:In 2\" }") != std::string::npos && // bass bypasses the HRTF
         conf.find("{ output = \"lp1:Out\" input = \"mixL:In 2\" }") != std::string::npos &&
         conf.find("{ output = \"mixL:Out\" input = \"eqL0:In\" }") != std::string::npos &&
         conf.find("outputs = [ \"ldL0:Out\" \"ldR0:Out\" ]") != std::string::npos &&
         conf.find("target.object = \"bluez_output.AA_BB.1\"") != std::string::npos);
    // Bass Booster's bands add up to +6.48 dB (largest band 5.5), the spatial path adds 10.4 dB
    Test("Chain: summed boost and spatial peak are taken off by the pre-gain", conf.find("name = preL control = { Freq = 0.00 Q = 1.00 Gain = -16.") != std::string::npos);
    auto custom = config;
    custom.sofa = "/home/me/personal.sofa";
    auto customConf = AudioEffects::BuildConfig("s", "d", custom, 0);
    Test("Chain: custom SOFA is used without the KEMAR fixes", customConf.find("filename = \"/home/me/personal.sofa\"") != std::string::npos &&
                                                                      customConf.find("scL0") == std::string::npos &&
                                                                      customConf.find("name = lp0") == std::string::npos);

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
         xconf.find("{ output = \"preR:Out\" input = \"xlR:In\" }") != std::string::npos &&
         xconf.find("{ output = \"xlR:Out\" input = \"xmL:In 3\" }") != std::string::npos &&
         xconf.find("{ output = \"xlL:Out\" input = \"xmL:In 2\" }") != std::string::npos &&
         xconf.find("{ output = \"xmR:Out\" input = \"eqR0:In\" }") != std::string::npos &&
         AudioEffects::ControlCommand(crossfeed, 0).find("\"xmL:Gain 2\" -0.37 \"xmL:Gain 3\" 0.37") != std::string::npos);
    crossfeed.crossfeed = false;
    Test("Crossfeed off: nodes stay, low-passed paths muted", AudioEffects::ControlCommand(crossfeed, 0).find("\"xmL:Gain 2\" 0.00 \"xmL:Gain 3\" 0.00") != std::string::npos &&
                                                             AudioEffects::BuildConfig("s", "d", crossfeed, 0).find("\"Gain 2\" = 0.00 \"Gain 3\" = 0.00") != std::string::npos);

    EffectsConfig corrected;
    corrected.correction = AapDevice::MeasuredCorrection(static_cast<unsigned short>(AapModelIds::airpodsmax));
    Test("Correction: AirPods Max measured, USB-C shares it, Pro 3 has none", corrected.correction.size() == 10 &&
         AapDevice::MeasuredCorrection(static_cast<unsigned short>(AapModelIds::airpodsmax2024)) == corrected.correction &&
         AapDevice::MeasuredCorrection(static_cast<unsigned short>(AapModelIds::airpodspro3)).empty());
    Test("Correction off: neutral, nodes at 0 dB", corrected.IsNeutral() && AudioEffects::ControlCommand(corrected, 0).find("\"coL0:Gain\" 0.00") != std::string::npos);
    corrected.corrected = true;
    Test("Correction on: filters after the EQ with their gains", !corrected.IsNeutral() &&
         AudioEffects::BuildConfig("s", "d", corrected, 0).find("label = bq_lowshelf name = coL0 control = { Freq = 105.00 Q = 0.70 Gain = -3.00 }") != std::string::npos &&
         AudioEffects::ControlCommand(corrected, 0).find("\"coR5:Gain\" -5.50") != std::string::npos);
    Test("Chain: EQ gains are live controls", AudioEffects::ControlCommand(config, 0).find("\"eqR0:Gain\" 5.50") != std::string::npos &&
                                              AudioEffects::BuildConfig("s", "d", EffectsConfig{SpatialMode::Fixed, {}}, 0).find("name = eqL0") != std::string::npos);
    Test("Chain: device name cannot break the config", conf.find("AirPods") != std::string::npos && conf.find("Air\"Pods") == std::string::npos);

    // Limiter: last in the chain, and spatial audio then needs only 3 dB instead of the 10.4 dB worst case
    EffectsConfig limited;
    limited.spatial = SpatialMode::Fixed;
    limited.limiter = "/usr/lib/ladspa/fast_lookahead_limiter_1913.so";
    auto lconf = AudioEffects::BuildConfig("s", "d", limited, 0);
    Test("Limiter: after the ear filters, the chain's outputs",
         lconf.find("type = ladspa name = lim plugin = \"/usr/lib/ladspa/fast_lookahead_limiter_1913.so\" label = fastLookaheadLimiter") != std::string::npos &&
         lconf.find("{ output = \"ldR0:Out\" input = \"lim:Input 2\" }") != std::string::npos &&
         lconf.find("outputs = [ \"lim:Output 1\" \"lim:Output 2\" ]") != std::string::npos);
    Test("Limiter: spatial pre-gain 3 dB instead of 10.4", std::abs(AudioEffects::HeadroomDb(limited) - 3.0) < 0.01 &&
                                                          std::abs(AudioEffects::HeadroomDb(EffectsConfig{SpatialMode::Fixed}) - 10.4) < 0.01);

    // AutoEQ's own format; the preamp is ignored (the headroom is computed), OFF filters skipped
    auto parsed = AudioEffects::ParseParametricEq("Preamp: -6.2 dB\r\nFilter 1: ON LSC Fc 105 Hz Gain 5.6 dB Q 0.70\n"
                                                  "Filter 2: OFF PK Fc 200 Hz Gain 9 dB Q 1\nFilter 3: ON PK Fc 2140 Hz Gain -3.4 dB Q 1.91\n"
                                                  "Filter 4: ON HSC Fc 10000 Hz Gain -2.3 dB Q 0.70\n");
    Test("ParametricEQ.txt: filters, types and values", parsed && parsed->size() == 3 && (*parsed)[0] == Biquad{Biquad::LowShelf, 105, 5.6, 0.7} &&
                                                        (*parsed)[1] == Biquad{Biquad::Peaking, 2140, -3.4, 1.91} && (*parsed)[2].type == Biquad::HighShelf);
    Test("ParametricEQ.txt: refuses junk, per-channel files and wild values",
         !AudioEffects::ParseParametricEq("") && !AudioEffects::ParseParametricEq("hello") &&
         !AudioEffects::ParseParametricEq("Channel: L\nFilter 1: ON PK Fc 100 Hz Gain 1 dB Q 1\n") &&
         !AudioEffects::ParseParametricEq("Filter 1: ON XX Fc 100 Hz Gain 1 dB Q 1\n") &&
         !AudioEffects::ParseParametricEq("Filter 1: ON PK Fc 100 Hz Gain 90 dB Q 1\n") &&
         !AudioEffects::ParseParametricEq("Filter 1: ON PK Fc abc Hz Gain 1 dB Q 1\n"));

    // Loudness: ISO 226 reproduces its own definition at 1 kHz, the shelf follows the contours
    Test("ISO 226: a contour passes its phon value at 1 kHz", std::abs(AudioEffects::Iso226(1000, 40) - 40) < 0.1 && std::abs(AudioEffects::Iso226(1000, 80) - 80) < 0.1);
    EffectsConfig loud;
    loud.loudness = true;
    Test("Loudness: flat at full volume, still a chain (it follows the volume)", AudioEffects::LoudnessShelf(loud).gain == 0 && !loud.IsNeutral());
    // below 50 phon the 15 dB cap takes over
    bool follows = true;
    for (double phon : {50.0, 60.0, 70.0})
    {
        loud.volume = std::pow(10.0, (phon - loud.loudnessReference) / 60); // 90 phon at 100 %, 60 dB per decade
        auto shelf = AudioEffects::LoudnessShelf(loud);
        for (double f : {31.5, 63.0, 125.0, 250.0, 500.0})
        {
            double iso = (AudioEffects::Iso226(f, phon) - AudioEffects::Iso226(f, 80)) - (phon - 80);
            follows &= std::abs(AudioEffects::GainDb(shelf, f) - AudioEffects::GainDb(shelf, 1000) - iso) < 2;
        }
        follows &= std::abs(AudioEffects::GainDb(shelf, 1000)) < 0.6;
    }
    Test("Loudness: shelf within 2 dB of ISO 226 from 31.5 Hz to 500 Hz", follows);
    loud.volume = 0.01;
    char shelfGain[48];
    std::snprintf(shelfGain, sizeof shelfGain, "\"ldL0:Gain\" %.2f", AudioEffects::LoudnessShelf(loud).gain);
    Test("Loudness: capped at 15 dB, the pre-gain takes it off", AudioEffects::LoudnessShelf(loud).gain == 15 &&
                                                                AudioEffects::ControlCommand(loud, 0).find(shelfGain) != std::string::npos &&
                                                                std::abs(AudioEffects::HeadroomDb(loud) - 15) < 0.3);

    // Hearing profile: half-gain rule per ear, at the ear
    auto audiogram = AudioEffects::ParseAudiogram("20, 25 30;40 55 60");
    Test("Audiogram: six thresholds, separators, refuses the rest", audiogram == std::array<double, 6>{20, 25, 30, 40, 55, 60} &&
                                                                  !AudioEffects::ParseAudiogram("1 2 3") && !AudioEffects::ParseAudiogram("1 2 3 4 5 6 7") &&
                                                                  !AudioEffects::ParseAudiogram("1 2 3 4 5 500") && !AudioEffects::ParseAudiogram(""));
    Test("Audiogram: half-gain rule, capped at 20 dB", AudioEffects::HearingGains(*audiogram) == std::array<double, 6>{10, 12.5, 15, 20, 20, 20} &&
                                                      AudioEffects::HearingGains({-10, 0, 0, 0, 0, 0})[0] == 0);
    EffectsConfig ears;
    ears.hearing[1][5] = 10;
    auto econf = AudioEffects::BuildConfig("s", "d", ears, 0);
    Test("Hearing profile: right ear only", !ears.IsNeutral() && econf.find("name = hlR5 control = { Freq = 8000.00 Q = 1.41 Gain = 10.00 }") != std::string::npos &&
                                            econf.find("name = hlL5 control = { Freq = 8000.00 Q = 1.41 Gain = 0.00 }") != std::string::npos &&
                                            std::abs(AudioEffects::HeadroomDb(ears) - 10) < 0.3);

    // 7.1: eight inputs, seven virtual speakers, the LFE joins the bass path
    EffectsConfig surround;
    surround.spatial = SpatialMode::HeadTracked;
    surround.surround = true;
    auto sconf = AudioEffects::BuildConfig("s", "d", surround, 0);
    Test("Surround: 7.1 sink, LFE into the bass, no LFE speaker",
         sconf.find("audio.channels = 8 audio.position = [ FL FR FC LFE RL RR SL SR ]") != std::string::npos &&
         sconf.find("{ output = \"preLFE:Out\" input = \"bass:In 4\" }") != std::string::npos &&
         sconf.find("{ output = \"spSR:Out R\" input = \"hrtfR:In 7\" }") != std::string::npos &&
         sconf.find("name = spLFE") == std::string::npos && sconf.find("name = spSL config") != std::string::npos);
    Test("Surround: head tracking turns every speaker", AudioEffects::ControlCommand(surround, 10).find("\"spRL:Azimuth\" 145.00") != std::string::npos &&
                                                      AudioEffects::ControlCommand(surround, 10).find("\"spC:Azimuth\" 10.00") != std::string::npos);
    Test("Surround: 3 dB more headroom than stereo", std::abs(AudioEffects::HeadroomDb(surround) - 15.2) < 0.01);
    Test("Surround off: stereo sink", AudioEffects::BuildConfig("s", "d", EffectsConfig{SpatialMode::Fixed}, 0).find("audio.channels = 2 audio.position = [ FL FR ]") != std::string::npos);

    // A/B: effects off, the pre-gain stays
    EffectsConfig ab;
    ab.eq = *AudioEffects::Preset("Bass Booster");
    ab.crossfeed = true;
    auto on = AudioEffects::ControlCommand(ab, 0);
    ab.bypass = true;
    auto off = AudioEffects::ControlCommand(ab, 0);
    Test("A/B: filters and crossfeed off, same pre-gain", ab.IsNeutral() == false && on.find("\"eqL0:Gain\" 5.50") != std::string::npos &&
                                                       off.find("\"eqL0:Gain\" 0.00") != std::string::npos && off.find("\"xmL:Gain 3\" 0.00") != std::string::npos &&
                                                       on.substr(0, on.find("eqL0")) == off.substr(0, off.find("eqL0")));
    surround.bypass = true;
    auto bconf = AudioEffects::BuildConfig("s", "d", surround, 0);
    Test("A/B: spatial audio swaps to the unprocessed (downmixed) input",
         bconf.find("{ output = \"dmL:Out\" input = \"mixL:In 3\" }") != std::string::npos &&
         AudioEffects::ControlCommand(surround, 0).find("\"mixL:Gain 1\" 0.00 \"mixL:Gain 2\" 0.00 \"mixL:Gain 3\" 1.00") != std::string::npos);
}

void TestsAapAudio::Test(const char *name, bool ok)
{
    if (!ok)
        failures++;
    Logger::Info("%-50s %s", name, ok ? "OK" : "FAILED");
}
