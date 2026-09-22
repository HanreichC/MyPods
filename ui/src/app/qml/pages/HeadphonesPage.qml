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

    title: qsTrId("menu.headphones")

    property var btAdapterData: ({})
    property var headphonesData: []
    readonly property var sortedHeadphones: hasHeadphones ? headphonesData.slice().sort(function (a, b) {
        return (a.name || "").localeCompare(b.name || "");
    }) : []
    readonly property bool hasHeadphones: Object.keys(headphonesData).length > 0
    readonly property bool hasBtAdapter: Object.keys(btAdapterData).length > 0

    function requestDevicesData() {
        if (cppBackend && cppBackend.connected) {
            cppBackend.getDefaultBluetoothAdapter();
            cppBackend.getDevices();
        }
    }

    Connections {
        target: cppBackend
        enabled: !!cppBackend
        function onDataReceived(json) {
            if (!json || Object.keys(json).length === 0) {
                rootPage.headphonesData = [];
            } else if (json.headphones) {
                rootPage.headphonesData = json.headphones;
            }

            if (!json || Object.keys(json).length === 0) {
                rootPage.btAdapterData = ({});
            } else if (json.defaultbluetooth) {
                rootPage.btAdapterData = json.defaultbluetooth;
            }
        }
        function onConnectedChanged() {
            requestDevicesData();
        }
    }
    Component.onCompleted: {
        requestDevicesData();
    }

    Components.HelpMessage {
        visible: !hasBtAdapter
        iconSource: MP.Theme.asset("icons/illustration-bluetooth-off.svg")
        titleText: qsTrId("headphones.help.bluetooth_not_found.header")
        bodyText: qsTrId("headphones.help.bluetooth_not_found.description")
    }

    Components.Card {
        visible: rootPage.hasBtAdapter

        MP.FormRow {
            label: qsTrId("headphones.item.bluetooth")
            iconSource: MP.Theme.asset("icons/icon-bluetooth.svg")

            Components.Toggle {
                id: bt
                checked: rootPage.btAdapterData?.enabled ?? false
                onToggled: {
                    rootPage.btAdapterData.enabled = checked;
                    if (checked)
                        cppBackend.enableDefaultBluetoothAdapter();
                    else
                        cppBackend.disableDefaultBluetoothAdapter();
                }
            }
        }
    }

    Components.HelpMessage {
        visible: !hasHeadphones && hasBtAdapter
        iconSource: MP.Theme.asset("icons/illustration-pair.svg")
        titleText: qsTrId("headphones.help.bluetooth_no_paired_headphones.header")
        bodyText: qsTrId("headphones.help.no_paired_headphones.description")
    }

    MP.Heading {
        visible: hasHeadphones && hasBtAdapter
        level: 5
        Layout.topMargin: MP.Units.largeSpacing
        Layout.leftMargin: MP.Units.largeSpacing
        text: qsTrId("headphones.headphones")
    }

    Components.Card {
        visible: hasHeadphones && hasBtAdapter

        Repeater {
            model: rootPage.sortedHeadphones
            delegate: MP.FormRow {
                enabled: bt.checked
                label: modelData.name
                iconSource: MP.Theme.asset("icons/icon-headphones.svg")
                iconColor: modelData.connected ? MP.Theme.accent : MP.Theme.gray

                Components.Toggle {
                    checked: modelData.connected
                    onToggled: {
                        if (modelData.address) {
                            modelData.connected = checked;
                            if (checked)
                                cppBackend.connectDevice(modelData.address);
                            else
                                cppBackend.disconnectDevice(modelData.address);
                        }
                    }
                }
            }
        }
    }
}
