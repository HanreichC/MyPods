// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import mypods as MP

// Desktop text styles like MyCAD's: 1 = page title, 2 = title, 3 = headline, 4 = body emphasized,
// 5 = section header (small caps in grey, like MyCAD's LIBRARY)
Text {
    id: root
    property int level: 1
    color: level === 5 ? MP.Theme.secondaryText : MP.Theme.text
    font.pixelSize: ({ 1: MP.Theme.titleSize, 2: 20, 3: 15, 4: MP.Theme.bodySize })[level] ?? MP.Theme.sectionSize
    font.weight: level <= 2 ? Font.Bold : Font.DemiBold
    font.letterSpacing: ({ 1: -0.4, 2: -0.3, 5: 0.5 })[level] ?? 0
    font.capitalization: level === 5 ? Font.AllUppercase : Font.MixedCase
}
