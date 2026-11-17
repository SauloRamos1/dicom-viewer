// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import DicomViewer

// Frameless startup window. Main.qml drives `progress` and `status`.
Window {
    id: root

    property real progress: 0
    property string status: ""

    signal faded

    function fadeOut() {
        fade.start()
    }

    width: 480
    height: 300
    // Centred on the primary screen; the window's own Screen would loop, as moving
    // the window can change which screen it is on.
    readonly property var primaryScreen: Application.screens[0]
    x: primaryScreen ? primaryScreen.virtualX + (primaryScreen.width - width) / 2 : 0
    y: primaryScreen ? primaryScreen.virtualY + (primaryScreen.height - height) / 2 : 0
    flags: Qt.SplashScreen | Qt.FramelessWindowHint
    color: "transparent"

    NumberAnimation {
        id: fade
        target: root
        property: "opacity"
        to: 0
        duration: Theme.durationSlow
        easing.type: Theme.easing
        onFinished: {
            root.close()
            root.faded()
        }
    }

    Pane {
        anchors.fill: parent
        padding: 32
        Material.theme: Theme.materialTheme
        Material.accent: Theme.accent
        Material.background: Theme.surface
        Material.foreground: Theme.text

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Item { Layout.fillHeight: true }

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "icons/app.svg"
                sourceSize: Qt.size(96, 96)
                opacity: root.visible ? 1 : 0
                scale: root.visible ? 1 : 0.9
                Behavior on opacity {
                    OpacityAnimator { duration: Theme.durationSlow; easing.type: Theme.easing }
                }
                Behavior on scale {
                    ScaleAnimator { duration: Theme.durationSlow; easing.type: Theme.easing }
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: AppInfo.appName
                font.pixelSize: 24
                font.weight: Font.Medium
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("v%1 - Qt %2 - DCMTK %3")
                    .arg(AppInfo.version).arg(AppInfo.qtVersion).arg(AppInfo.dcmtkVersion)
                color: Theme.textMuted
            }

            Item { Layout.fillHeight: true }

            ProgressBar {
                Layout.fillWidth: true
                value: root.progress
                Behavior on value {
                    NumberAnimation { duration: Theme.durationNormal; easing.type: Theme.easing }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: root.status
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: Theme.textMuted
                }
                Label {
                    text: qsTr("Not for diagnostic use")
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.warning
                }
            }
        }
    }
}
