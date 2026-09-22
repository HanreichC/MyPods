// MyPods
// License: GPL-3.0

#include "TestsZik.h"
#include "sdk/zik/ZikProtocol.h"
#include "device/capabilities/zik/ZikCapabilities.h"
#include "Logger.h"

using namespace MagicPodsCore;

// A Zik answer frame as it comes over the air: header, 01 01, length again, XML
static std::vector<unsigned char> Answer(const std::string &xml)
{
    auto frame = Zik::Frame(Zik::Request, std::string("\x01\x01", 2) + std::string(2, '\0') + xml);
    frame[5] = frame[0];
    frame[6] = frame[1];
    return frame;
}

TestsZik::TestsZik()
{
    Test("Zik open session", Zik::Frame(Zik::OpenSession) == std::vector<unsigned char>{0x00, 0x03, 0x00});

    // zik2ctl/pyParrotZik: length includes the 3 header bytes, then "GET <query>"
    auto req = Zik::EncodeRequest("/api/system/battery/get");
    Test("Zik request header", req.size() == 3 + 27 && req[0] == 0 && req[1] == 30 && req[2] == 0x80);
    Test("Zik request payload", std::string(req.begin() + 3, req.end()) == "GET /api/system/battery/get");

    const std::string battery = R"(<?xml version="1.0" encoding="UTF-8" ?><answer path="/api/system/battery/get"><system><battery state="charging" percent="74" timeleft=""/></system></answer>)";
    const std::string notify = R"(<notify path="/api/audio/noise_control/get"/>)";

    // split across reads and two frames glued together: RFCOMM gives no message boundaries
    Zik::Framer framer;
    auto a = Answer(battery), n = Answer(notify);
    std::vector<unsigned char> first(a.begin(), a.begin() + 10), rest(a.begin() + 10, a.end());
    rest.insert(rest.end(), n.begin(), n.end());
    rest.insert(rest.end(), {0x00, 0x03, 0x02});
    Test("Zik framer waits for a whole frame", framer.Feed(first).empty());
    auto msgs = framer.Feed(rest);
    Test("Zik framer splits glued frames", msgs.size() == 3 && msgs[0].xml == battery && msgs[1].xml == notify && msgs[2].id == Zik::Ack);

    Test("Zik attr", Zik::Attr(battery, "battery", "percent") == "74" && Zik::Attr(battery, "battery", "state") == "charging");
    Test("Zik attr missing", !Zik::Attr(battery, "battery", "level") && !Zik::Attr(battery, "noise_control", "type"));
    Test("Zik attr is per element", Zik::Attr(R"(<audio><noise_control enabled="true"/><equalizer enabled="false"/></audio>)", "equalizer", "enabled") == "false");
    Test("Zik invalid_on is on", Zik::BoolAttr(R"(<anc_phone_mode enabled="invalid_on"/>)", "anc_phone_mode", "enabled") == true);
    Test("Zik notify path", Zik::Attr(notify, "notify", "path") == "/api/audio/noise_control/get");

    Test("Zik equalizer off is flat", ZikEqualizerCapability::ThumbEqualizerArg("Off") == "0.0,0.0,0.0,0.0,0.0,0,0");
    // Bass Booster {5.5, 4.25, 3.5, 2.5, 1.25, 0...}: band pairs averaged, then doubled
    Test("Zik equalizer bass booster", ZikEqualizerCapability::ThumbEqualizerArg("Bass Booster") == "9.8,6.0,1.2,0.0,0.0,0,0");

    // Captured from a Zik 2 (fw 2.05) after noise_control/set?arg=off: an answer carrying a notify
    // must still complete the request, or the queue stalls
    const std::string answerWithNotify = R"(<?xml version="1.0" encoding="UTF-8"?><answer path="/api/audio/noise_control/set?arg"><notify path="/api/audio/noise_control/enabled/get"/></answer>)";
    Test("Zik answer with notify is an answer", Zik::IsAnswer(answerWithNotify) && Zik::Attr(answerWithNotify, "notify", "path") == "/api/audio/noise_control/enabled/get");
    Test("Zik bare notify is not an answer", !Zik::IsAnswer(notify));
    // tts has its own element; tts="none" on <software> must not be read as the switch
    Test("Zik tts element", Zik::BoolAttr(R"(<answer path="/api/software/tts/get"><tts enabled="true"/></answer>)", "tts", "enabled") == true
                            && !Zik::Attr(R"(<software sip6="2.05" pic="5" tts="none"/>)", "tts", "enabled"));
}

void TestsZik::Test(const char *name, bool ok)
{
    if (!ok)
        failures++;
    Logger::Info("%-50s %s", name, ok ? "OK" : "FAILED");
}
