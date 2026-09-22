// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import magicpods as MP

// HIG text styles: 1 = Large Title, 2 = Title 2, 3 = Headline, 4 = Body emphasized, 5 = grouped section header (Footnote)
Text {
    id: root
    property int level: 1
    color: level === 5 ? MP.Theme.secondaryText : MP.Theme.text
    font.pixelSize: ({ 1: 34, 2: 22, 3: 17, 4: 17 })[level] ?? 13
    font.weight: level === 5 ? Font.Normal : (level <= 2 ? Font.Bold : Font.DemiBold)
    font.letterSpacing: ({ 1: -0.7, 2: -0.4 })[level] ?? 0
    font.capitalization: level === 5 ? Font.AllUppercase : Font.MixedCase
}
