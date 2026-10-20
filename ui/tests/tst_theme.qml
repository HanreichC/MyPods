// MyStyle's text styles reach the text: a style with a change keeps its tracking, the detail style is 11 regular,
// and Units sees Theme as the module's singleton. Run like tst_picker.qml.
import QtQuick
import QtTest
import mypods as MP

Item {
    width: 400
    height: 200

    MP.Button { id: button; text: "Connect" }
    MP.Heading { id: section; y: 40; level: 5; text: "General" }

    TestCase {
        name: "Theme"

        function test_styles() {
            compare(button.font.pixelSize, MP.Theme.fontCallout.pixelSize);
            compare(button.font.weight, Font.DemiBold);
            fuzzyCompare(button.font.letterSpacing, MP.Theme.fontCallout.letterSpacing, 0.02);
            compare(section.font.pixelSize, MP.Theme.fontSection.pixelSize);
            compare(section.font.capitalization, Font.AllUppercase);
            compare(MP.Units.fontDetail.pixelSize, 11);
            compare(MP.Units.fontDetail.weight, Font.Normal);
            compare(MP.Units.fontDetail.capitalization, Font.MixedCase);
            verify(Qt.colorEqual(MP.Units.batteryColor(10, false), MP.Theme.red));
            verify(Qt.colorEqual(MP.Units.batteryColor(80, false), MP.Theme.accent));
        }
    }
}
