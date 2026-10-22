// The lid popup: closed by hand it stays closed while the lid is open (battery updates keep coming), opens again
// after the lid was closed, and a second pair's lid never closes the one shown; Connect only without automatic
// switching. Run like tst_picker.qml.
import QtQuick
import QtTest
import mypods as MP

Item {
    MP.PopupAnimation {
        id: popup
    }

    TestCase {
        name: "PopupAnimation"

        function lid(address, show, battery) {
            popup.handleAnimation({ address: address, name: "AirPods", show: show, connected: false,
                                    battery: { single: { battery: battery ?? 50, charging: false, status: 2 } } });
        }

        function test_dismissedStaysClosedUntilTheLidCloses() {
            lid("A", true);
            verify(popup.animationShown, "lid opened");

            popup.dismiss();
            tryCompare(popup, "animationShown", false);
            lid("A", true, 49);
            verify(!popup.animationShown, "a battery update doesn't bring it back");

            lid("A", false);
            lid("A", true);
            verify(popup.animationShown, "lid closed and opened again");
        }

        function test_anotherPairNeitherTakesOverNorCloses() {
            lid("A", true);
            verify(popup.animationShown);
            lid("B", false);
            lid("B", true, 10);
            verify(popup.animationShown, "still shown");
            compare(popup.animationData.address, "A", "still the first pair");
            lid("A", false);
            tryCompare(popup, "animationShown", false);
        }

        function test_connectOnlyWithoutAutomaticSwitching() {
            let button = findChild(popup.contentItem, "connectButton");
            verify(button);
            let pair = (autoSwitch, connected) => popup.handleAnimation({ address: "C", name: "AirPods", show: true,
                connected: connected, autoSwitch: autoSwitch, battery: { single: { battery: 50, charging: false, status: 2 } } });
            pair(true, false);
            tryCompare(popup, "visible", true);
            verify(!button.visible, "automatic switching connects them on its own");
            pair(true, true);
            verify(button.visible, "connected: Disconnect stays");
            pair(false, false);
            verify(button.visible, "without automatic switching Connect is the way");
            popup.handleAnimation({ address: "C", show: false });
            tryCompare(popup, "animationShown", false);
        }
    }
}
