// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import DicomViewer

ToolBar {
    id: root

    property int errorCount: 0
    property alias metadataChecked: metadataButton.checked
    property alias errorsChecked: errorsButton.checked
    property alias optionsChecked: optionsButton.checked
    property alias view3DChecked: view3DButton.checked

    signal toggleTheme
    signal newRequested
    signal openRequested
    signal openFolderRequested

    implicitHeight: 48

    Shortcut {
        sequence: "Ctrl+Shift+O"
        onActivated: root.openFolderRequested()
    }

    background: Rectangle {
        color: Theme.surface
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 4

        IconButton {
            id: metadataButton
            glyph: FontAwesome.circleInfo
            tip: qsTr("Metadata")
            shortcut: "Ctrl+I"
            checkable: true
            checked: true
        }
        IconButton {
            glyph: FontAwesome.file
            tip: qsTr("New")
            shortcut: "Ctrl+N"
            onClicked: root.newRequested()
        }
        // Split button: the folder opens a file (Ctrl+O); the chevron's menu also
        // offers a folder as a series (Ctrl+Shift+O).
        IconButton {
            glyph: FontAwesome.folderOpen
            tip: qsTr("Open file")
            shortcut: "Ctrl+O"
            onClicked: root.openRequested()
        }
        IconButton {
            Layout.leftMargin: -4
            glyph: FontAwesome.chevronDown
            tip: qsTr("Open file or folder")
            tipSuppressed: openMenu.visible
            implicitWidth: 24
            onClicked: openMenu.open()

            Menu {
                id: openMenu
                y: parent.height + 6

                enter: Transition {
                    OpacityAnimator {
                        from: 0; to: 1
                        duration: Theme.durationFast; easing.type: Theme.easing
                    }
                }
                exit: Transition {
                    OpacityAnimator {
                        to: 0
                        duration: Theme.durationFast; easing.type: Theme.easing
                    }
                }
                background: Rectangle {
                    implicitWidth: 224
                    radius: 8
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                }

                MenuItem {
                    text: qsTr("Open file...")
                    onTriggered: root.openRequested()
                }
                MenuItem {
                    text: qsTr("Open folder (series)...")
                    onTriggered: root.openFolderRequested()
                }
            }
        }
        IconButton {
            id: optionsButton
            glyph: FontAwesome.sliders
            tip: qsTr("Options")
            shortcut: "Ctrl+,"
            checkable: true
        }
        IconButton {
            id: view3DButton
            glyph: FontAwesome.cube
            tip: qsTr("3D View")
            shortcut: "Ctrl+3"
            checkable: true
        }

        Item { Layout.fillWidth: true }

        IconButton {
            glyph: FontAwesome.language
            tip: qsTr("Language")
            tipSuppressed: languageMenu.visible
            onClicked: languageMenu.open()

            Menu {
                id: languageMenu
                x: parent.width - width
                y: parent.height + 6

                enter: Transition {
                    OpacityAnimator {
                        from: 0; to: 1
                        duration: Theme.durationFast; easing.type: Theme.easing
                    }
                }
                exit: Transition {
                    OpacityAnimator {
                        to: 0
                        duration: Theme.durationFast; easing.type: Theme.easing
                    }
                }
                background: Rectangle {
                    implicitWidth: 192
                    radius: 8
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                }

                MenuItem {
                    text: "English"
                    checkable: true
                    autoExclusive: true
                    checked: LanguageManager.language === "en"
                    onTriggered: LanguageManager.language = "en"
                }
                MenuItem {
                    text: "Português (BR)"
                    checkable: true
                    autoExclusive: true
                    checked: LanguageManager.language === "pt_BR"
                    onTriggered: LanguageManager.language = "pt_BR"
                }
            }
        }
        IconButton {
            glyph: Theme.dark ? FontAwesome.sun : FontAwesome.moon
            tip: Theme.dark ? qsTr("Switch to light theme") : qsTr("Switch to dark theme")
            shortcut: "Ctrl+T"
            onClicked: root.toggleTheme()
        }
        IconButton {
            id: errorsButton
            glyph: FontAwesome.triangleExclamation
            tip: qsTr("Errors")
            shortcut: "Ctrl+E"
            badgeCount: root.errorCount
            checkable: true
            checked: true
        }
    }
}
