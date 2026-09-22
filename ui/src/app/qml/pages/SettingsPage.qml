// MagicPodsLinux: https://github.com/steam3d/MagicPodsLinux
// Copyright: 2020-2026 Aleksandr Maslov <https://magicpods.app>
// License: GPL-3.0

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import magicpods as MP
import "../components" as Components

Components.ScrollPage {
    id: rootPage

    property bool settingAnimation: true

    readonly property int mWidth: MP.Units.gridUnit * 8
    title: qsTrId("menu.settings")

    // "auto" | "light" | "dark" <-> picker index; shared by Appearance and Tray icon theme
    function themeTrayStringToIndex(value) {
        if (value === "light") return 1;
        if (value === "dark") return 2;
        return 0;
    }

    function themeTrayIndexToString(index) {
        if (index === 1) return "light";
        if (index === 2) return "dark";
        return "auto";
    }

    function requestSettings() {
        if (cppBackend && cppBackend.connected) {
            cppBackend.getSetting("magicpods", "animation");
            cppBackend.getSetting("magicpods", "theme_tray");
            cppBackend.getSetting("magicpods", "appearance");
        }
    }

    Connections {
        target: cppBackend
        enabled: !!cppBackend
        function onDataReceived(json) {
            if (!json || Object.keys(json).length === 0) {} else if (json.settings) {
                if (json.settings?.magicpods?.animation != null)
                    rootPage.settingAnimation = json.settings.magicpods.animation;
                if (json.settings?.magicpods?.theme_tray != null && cppTrayIcon)
                    cppTrayIcon.themeMode = rootPage.themeTrayStringToIndex(json.settings.magicpods.theme_tray);
                if (json.settings?.magicpods?.appearance != null)
                    MP.Theme.appearance = rootPage.themeTrayIndexToString(rootPage.themeTrayStringToIndex(json.settings.magicpods.appearance));
            }
        }
        function onConnectedChanged() {
            requestSettings();
        }
    }
    Component.onCompleted: {
        requestSettings();
    }

    MP.Heading {
        level: 5
        Layout.topMargin: MP.Units.mediumSpacing
        Layout.leftMargin: MP.Units.largeSpacing
        text: qsTrId("settings.popup")
    }

    Components.Card {

        MP.FormRow {
            iconSource: MP.Theme.asset("icons/icon-sparkles.svg")
            iconColor: "#AF52DE"
            label: qsTrId("settings.headphones_animation")
            tooltip: qsTrId("settings.headphones_animation.description")

            Components.Toggle {
                checked: settingAnimation
                enabled: cppBackend?.connected ?? false
                onToggled: {
                    if (settingAnimation !== checked) {
                        settingAnimation = checked;
                        if (cppBackend)
                            cppBackend.setSetting("magicpods", "animation", checked);
                    }
                }
            }
        }
    }

    MP.Heading {
        level: 5
        Layout.topMargin: MP.Units.mediumSpacing
        Layout.leftMargin: MP.Units.largeSpacing
        text: qsTrId("settings.other")
    }

    Components.Card {

        MP.FormRow {
            iconSource: MP.Theme.asset("icons/icon-theme.svg")
            iconColor: MP.Theme.indigo
            label: qsTrId("settings.appearance")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                enabled: cppBackend?.connected ?? false
                model: [
                    qsTrId("settings.tray_icon_theme.auto"),
                    qsTrId("settings.tray_icon_theme.light"),
                    qsTrId("settings.tray_icon_theme.dark")
                ]
                currentIndex: rootPage.themeTrayStringToIndex(MP.Theme.appearance)
                onActivated: {
                    MP.Theme.appearance = rootPage.themeTrayIndexToString(currentIndex);
                    if (cppBackend)
                        cppBackend.setSetting("magicpods", "appearance", MP.Theme.appearance);
                }
            }
        }

        MP.FormRow {
            iconSource: MP.Theme.asset("icons/icon-apps.svg")
            iconColor: MP.Theme.accent
            label: qsTrId("settings.add_shortcut_to_menu")
            tooltip: qsTrId("settings.add_shortcut_to_menu.description")

            Components.Toggle {
                id: menuShortcutSwitch
                property bool skipToggle: false
                checked: false
                enabled: false

                function refreshState() {
                    skipToggle = true;
                    enabled = !!desktopManager;
                    checked = !!desktopManager && desktopManager.isInstalled();
                    skipToggle = false;
                }

                onToggled: {
                    if (skipToggle)
                        return;

                    if (checked) {
                        if (!desktopManager?.install())
                            refreshState();
                    } else {
                        if (!desktopManager?.uninstall())
                            refreshState();
                    }
                }

                Component.onCompleted: refreshState()
            }
        }

        MP.FormRow {
            iconSource: MP.Theme.asset("icons/icon-battery-fill.svg")
            iconColor: MP.Theme.gray
            label: qsTrId("settings.tray_icon_theme")

            Components.Picker {
                implicitWidth: rootPage.mWidth
                enabled: cppBackend?.connected ?? false
                model: [
                    qsTrId("settings.tray_icon_theme.auto"),
                    qsTrId("settings.tray_icon_theme.light"),
                    qsTrId("settings.tray_icon_theme.dark")
                ]
                currentIndex: cppTrayIcon ? cppTrayIcon.themeMode : 0
                onActivated: {
                    if (cppTrayIcon)
                        cppTrayIcon.themeMode = currentIndex;
                    if (cppBackend)
                        cppBackend.setSetting("magicpods", "theme_tray", rootPage.themeTrayIndexToString(currentIndex));
                }
            }
        }
    }
}
