// A new model (e.g. after a language switch) must not reset the picker to the first item.
// Run against the sources (needs a configured build/ for the generated qmldir):
//   d=$(mktemp -d) && mkdir $d/mypods && sed /^prefer/d build/qml/mypods/qmldir > $d/mypods/qmldir \
//     && ln -s $PWD/ui/src $d/mypods/src && QT_QPA_PLATFORM=offscreen qmltestrunner -import $d -input ui/tests/tst_picker.qml
import QtQuick
import QtTest
import mypods as MP

Item {
    width: 400; height: 400
    property int generation: 0
    property int selected: 1

    MP.Picker {
        id: picker
        model: ["a" + generation, "b" + generation, "c" + generation]
        currentIndex: selected
        onActivated: selected = currentIndex
    }

    TestCase {
        name: "Picker"
        when: windowShown

        function test_newModelKeepsSelection() {
            generation++;
            compare(picker.currentIndex, 1);
            compare(picker.displayText, "b1");

            picker.forceActiveFocus();
            keyClick(Qt.Key_Down);
            compare(selected, 2);
            generation++;
            compare(picker.currentIndex, 2, "after a user pick");

            selected = 0;
            compare(picker.currentIndex, 0, "caller's binding still alive");
        }

        // Qt 6.12's Material ComboBox drew the value white whatever Material.foreground said
        function test_valueReadableInBothAppearances() {
            for (const appearance of ["light", "dark"]) {
                MP.Theme.appearance = appearance;
                verify(Qt.colorEqual(picker.contentItem.color, MP.Theme.secondaryText), appearance + ": " + picker.contentItem.color);
            }
            MP.Theme.appearance = "auto";
        }
    }
}
