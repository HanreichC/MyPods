// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import mypods as MP

// Body text. Another style goes in whole (font: MP.Theme.fontCallout): a single font.pixelSize would drop this one.
Text {
    color: MP.Theme.labelPrimary
    font: MP.Theme.fontBody
}
