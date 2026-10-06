// The (i) beside a setting: a click opens what it does, Escape closes it again; a row without a text has none.
// Run like tst_picker.qml.
import QtQuick
import QtQuick.Controls
import QtTest
import magicpods as MP

Item {
    width: 500
    height: 500

    MP.FormRow {
        id: row
        width: 400
        label: "Equalizer:"
        info: "What the equalizer does."
    }
    MP.FormRow {
        id: plain
        y: 60
        width: 400
        label: "Name"
    }

    TestCase {
        name: "InfoButton"
        when: windowShown

        function findInfoButton(item) {
            if (item.info !== undefined && item.title !== undefined && item.clicked !== undefined)
                return item;
            for (let i = 0; i < item.children.length; i++) {
                const found = findInfoButton(item.children[i]);
                if (found)
                    return found;
            }
            return null;
        }

        function test_opensAndCloses() {
            const button = findInfoButton(row);
            verify(button && button.visible, "the row with a text has its (i)");
            compare(button.title, "Equalizer:");
            const popup = button.children.find(c => c.opened !== undefined) ?? button.data.find(c => c.opened !== undefined);
            verify(popup, "the button holds its popup");
            mouseClick(button);
            tryCompare(popup, "opened", true);
            keyClick(Qt.Key_Escape);
            tryCompare(popup, "opened", false);
        }

        function test_noTextNoButton() {
            const button = findInfoButton(plain);
            verify(!button || !button.visible, "a row without a text shows no (i)");
        }
    }
}
