// Standalone self-check for the keyboard shortcut actions (not part of the UI build):
//   g++ -std=c++20 -fPIC -Iui/src/app/cpp ui/tests/ActionsCheck.cpp $(pkg-config --cflags --libs Qt6Core) -o /tmp/actions && /tmp/actions

#include <cassert>
#include <cstdio>
#include "Actions.h"

static QJsonObject Info(int anc, int options, bool ca)
{
    return {{"address", "AA:BB"},
            {"capabilities", QJsonObject{{"anc", QJsonObject{{"selected", anc}, {"options", options}}},
                                         {"conversationAwareness", QJsonObject{{"selected", ca}}}}}};
}

static QJsonObject Change(const QJsonObject &request)
{
    return request.value("arguments").toObject().value("capabilities").toObject();
}

int main()
{
    // AirPods Pro: off, transparency, adaptive, ANC (1 | 2 | 4 | 16)
    assert(Actions::nextNoiseMode(16, 1 | 2 | 4 | 16) == 1); // wraps around
    assert(Actions::nextNoiseMode(2, 2 | 16) == 16);          // skips modes the model lacks
    assert(Actions::nextNoiseMode(2, 2) == -1);               // nothing to switch to

    auto next = Actions::request("noise-next", Info(2, 2 | 16, false));
    assert(next.value("method").toString() == "SetCapabilities");
    assert(next.value("arguments").toObject().value("address").toString() == "AA:BB");
    assert(Change(next).value("anc").toObject().value("selected").toInt() == 16);

    assert(Actions::request("noise-adaptive", Info(2, 2 | 16, false)).isEmpty()); // no adaptive on this model
    assert(Change(Actions::request("conversation-awareness", Info(2, 2, true))).value("conversationAwareness").toObject().value("selected") == false);
    assert(Actions::request("move-here", Info(2, 2, true)).isEmpty()); // no automatic switching offered
    assert(Actions::request("noise-next", QJsonObject{}).isEmpty());  // no headphones connected

    // Equalizer: next preset wraps around, an unknown one starts at the first; A/B toggles
    auto eqInfo = [](const char *selected, bool bypass) {
        return QJsonObject{{"address", "AA:BB"}, {"capabilities", QJsonObject{{"equalizer", QJsonObject{
            {"selected", selected}, {"options", QJsonArray{"Off", "Rock", "Custom"}}, {"bypass", bypass}}}}}};
    };
    auto eqSelected = [](const QJsonObject &request) { return Change(request).value("equalizer").toObject().value("selected").toString(); };
    assert(eqSelected(Actions::request("eq-next", eqInfo("Off", false))) == "Rock");
    assert(eqSelected(Actions::request("eq-next", eqInfo("Custom", false))) == "Off");
    assert(eqSelected(Actions::request("eq-next", eqInfo("Gone", false))) == "Off");
    assert(Change(Actions::request("effects-bypass", eqInfo("Off", true))).value("equalizer").toObject().value("bypass") == false);
    assert(Actions::request("eq-next", Info(2, 2, true)).isEmpty()); // headphones without effects

    std::puts("ActionsCheck: OK");
    return 0;
}
