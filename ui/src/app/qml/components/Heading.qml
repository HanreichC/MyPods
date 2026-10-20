// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick
import mypods as MP

// MyStyle's desktop text styles: 1 = page title (Title 1), 2 = title (Title 2), 3 = headline, 4 = body emphasized,
// 5 = section header (small caps in grey)
Text {
    id: root
    property int level: 1
    color: level === 5 ? MP.Theme.labelSecondary : MP.Theme.labelPrimary
    font: [MP.Theme.fontTitle1, MP.Theme.fontTitle2, MP.Theme.fontHeadline, MP.Units.font(MP.Theme.fontBody, { weight: Font.DemiBold })][level - 1]
          ?? MP.Units.font(MP.Theme.fontSection, { features: { "case": 1 } }) // T-6: Inter's forms for capitals
}
