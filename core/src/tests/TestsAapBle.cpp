// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "TestsAapBle.h"
#include "device/capabilities/aap/AppAnimationCapability.h"
#include "dbus/DBusDeviceInfo.h"
#include "StringUtils.h"
#include "Logger.h"
#include <cstring>
#include <optional>
#include <string>

using namespace MagicPodsCore;

TestsAapBle::TestsAapBle()
{
    // Synthetic key: the key of the device the private captures came from is not public,
    // so their 16-byte encrypted tail was re-encrypted with this key (first 11 bytes are real).
    std::string enc = "000102030405060708090a0b0c0d0e0f";

#ifdef _WIN32
    // AirPods Max A2DP node; the vendor id follows its 4-digit source (0001 = Bluetooth SIG)
    Test("VID/PID from a Windows hardware id", DBusDeviceInfo::ParseVidPid(R"(BTHENUM\{0000110b-0000-1000-8000-00805f9b34fb}_VID&0001004c_PID&200a)") == std::array<unsigned short, 2>{0x004C, 0x200A} &&
                                               DBusDeviceInfo::ParseVidPid(R"(BTHENUM\{0000110b-0000-1000-8000-00805f9b34fb}_LOCALMFG&0002)") == std::array<unsigned short, 2>{0, 0});
#else
    Test("VID/PID from a BlueZ modalias", DBusDeviceInfo::ParseVidPid("bluetooth:v004Cp200Ad0001") == std::array<unsigned short, 2>{0x004C, 0x200A});
#endif
    Test("TestAirPodsMaxInEar_utp_22", TestAirPodsMaxInEar_utp_22());
    Test("TestAirPodsMaxPopupAnimation", TestAirPodsMaxPopupAnimation());
    Test("TestAirPodsMax2PopupAnimation", TestAirPodsMax2PopupAnimation());
    Test("TestAirPods2_utp_53", TestAirPods2_utp_53());
    Test("TestAirPods2_utp_33", TestAirPods2_utp_33());
    Test("TestBeatsSolo4", TestBeatsSolo4());
    Test("TestBeatsSoloBuds", TestBeatsSoloBuds());
    Test("TestPowerBeatsPro2", TestPowerBeatsPro2());
    Test("TestBeatsStudioBudsPlus", TestBeatsStudioBudsPlus());
    Test("TestBeatsStudioPro", TestBeatsStudioPro());
    Test("TestPowerBeatsPro", TestPowerBeatsPro());
    Test("TestPowerBeats4", TestPowerBeats4());
    Test("TestPrivateAirPods2_1", TestPrivateAirPods2_1(enc));
    Test("TestPrivateAirPods2_2", TestPrivateAirPods2_2(enc));
    Test("TestPrivateAirPods2_3", TestPrivateAirPods2_3(enc));
    Test("TestPrivateAirPods2_4", TestPrivateAirPods2_4(enc));
    Test("TestPrivateAirPods2_5", TestPrivateAirPods2_5(enc));
    Test("TestPrivateAirPods2_6", TestPrivateAirPods2_6(enc));
    Test("TestPrivateAirPods2_7", TestPrivateAirPods2_7(enc));
    Test("TestPrivateAirPods2_8", TestPrivateAirPods2_8(enc));
}

bool TestsAapBle::TestAirPodsMaxInEar_utp_22()
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x0f;
    expected.model = AapModelIds::airpodsmax;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Single,
        DeviceBatteryStatus::Connected,
        50,
        false);

    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010a20220580000f45e80d9bb8e51897326a99455bb1cff367"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestAirPodsMaxPopupAnimation()
{
    bleData expected;
    expected.animation = true;
    expected.color = 0x0f;
    expected.model = AapModelIds::airpodsmax;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Single,
        DeviceBatteryStatus::Connected,
        40,
        false);

    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010a20020480820f400185c65a0aff9097826bd7c542e1cc55"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

// Synthetic: Max popup capture with the Max 2 model id (2d 20), no real capture yet
bool TestsAapBle::TestAirPodsMax2PopupAnimation()
{
    bleData expected;
    expected.animation = true;
    expected.color = 0x0f;
    expected.model = AapModelIds::airpodsmax2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Single,
        DeviceBatteryStatus::Connected,
        40,
        false);

    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719012d20020480820f400185c65a0aff9097826bd7c542e1cc55"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestAirPods2_utp_53()
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        90,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        60,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f2053a9960200050185c65a0aff9097826bd7c542e1cc55"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestAirPods2_utp_33()
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        90,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        60,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f20339aa60100050185c65a0aff9097826bd7c542e1cc55"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestBeatsSolo4()
{
    // No popup animation for this model (not in IsAnimationSupport), so the advertisement is ignored
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("07190125200009800400041ed3edebd0bc052b11618fc29f861d8a"), static_cast<unsigned short>(AapModelIds::beatssolo4));
    return !actual.has_value();
}

bool TestsAapBle::TestBeatsSoloBuds()
{
    // No popup animation for this model (not in IsAnimationSupport), so the advertisement is ignored
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("071901262033898001030496bfb2c5f7b2365d80676c748ebcda28"), static_cast<unsigned short>(AapModelIds::beatssolobuds));
    return !actual.has_value();
}

bool TestsAapBle::TestPowerBeatsPro2()
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x01;
    expected.model = AapModelIds::powerbeatspro2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        90,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        100,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Disconnected,
        0,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719011d20019a8f02010489f29d3a59d0fb608af4215871356c34"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestBeatsStudioBudsPlus()
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::beatsstudiobudsplus;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Disconnected,
        0,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        10,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("07190116203cfa01060000db15f83d01f00da7fa30b667a1313ecd"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestBeatsStudioPro()
{
    // No popup animation for this model (not in IsAnimationSupport), so the advertisement is ignored
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719011720000600010100eb63a515bf0f416a06574cf37bc55a22"), static_cast<unsigned short>(AapModelIds::beatsstudiopro));
    return !actual.has_value();
}

bool TestsAapBle::TestPowerBeatsPro()
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x1f;
    expected.model = AapModelIds::powerbeatspro;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        100,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        50,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010b2013aaa5011f255b2524f9b6d960d3b047ff447133b345"), static_cast<unsigned short>(expected.model));
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPowerBeats4()
{
    // No popup animation for this model (not in IsAnimationSupport), so the advertisement is ignored
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010d20000780050105d8c07399be0547113083d7b84bde576e"), static_cast<unsigned short>(AapModelIds::powerbeats4));
    return !actual.has_value();
}

bool TestsAapBle::TestPrivateAirPods2_1(const std::string &enc)
{
    // extract private failed
    bleData expected;
    expected.animation = true;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        100,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        70,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f2075aab7010000fc5e182d7502ec13733292d933061b3c"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPrivateAirPods2_2(const std::string &enc)
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        100,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        72,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f2071aa97090004f1682c25b1e43ba332d0fac3a813273e"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPrivateAirPods2_3(const std::string &enc)
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        99,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        72,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f20719a97090004fcc2e67adfc621df0c37683762d219f0"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPrivateAirPods2_4(const std::string &enc)
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Disconnected,
        0,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        99,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Disconnected,
        0,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f2000f98f010004f16982f7f16c57736ffe918b3f848fd5"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPrivateAirPods2_5(const std::string &enc)
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        99,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        99,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        72,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f205199970a0004aff249324f3d2f2603ff68e99a89bff1"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPrivateAirPods2_6(const std::string &enc)
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        99,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Disconnected,
        0,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Disconnected,
        0,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f2021f98f010004f16982f7f16c57736ffe918b3f848fd5"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPrivateAirPods2_7(const std::string &enc)
{
    bleData expected;
    expected.animation = false;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        99,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        99,
        false);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Disconnected,
        0,
        false);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f2021998f01000485fd269fc7bbb7ce696f5e0b72829e39"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

bool TestsAapBle::TestPrivateAirPods2_8(const std::string &enc)
{
    bleData expected;
    expected.animation = true;
    expected.color = 0x00;
    expected.model = AapModelIds::airpods2;
    expected.batteryData.emplace_back(
        DeviceBatteryType::Left,
        DeviceBatteryStatus::Connected,
        100,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Right,
        DeviceBatteryStatus::Connected,
        100,
        true);
    expected.batteryData.emplace_back(
        DeviceBatteryType::Case,
        DeviceBatteryStatus::Connected,
        71,
        true);
    std::optional<bleData> actual = AppAnimationCapability::ParseBle(StringUtils::HexStringToBytes("0719010f2055aaf70100049e3459a875255504d6d25d7c5acab0ea"), static_cast<unsigned short>(expected.model), enc);
    return (actual.has_value() && actual.value() == expected);
}

void TestsAapBle::Test(const char *name, bool b)
{
    int spaceCount = 50 - std::strlen(name);
    std::string space = "";
    for (int i = 0; i < spaceCount; i++)
    {
        space += " ";
    }

    if (b)
    {
        Logger::Debug("%s%s: PASS", name, space.c_str());
    }
    else
    {
        failures++;
        Logger::Debug("%s%s: FAIL", name, space.c_str());
    }
}
