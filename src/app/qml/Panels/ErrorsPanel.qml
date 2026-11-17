// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DicomViewer

// Error cards: sample errors flagged "Example" at startup, the real errors of
// a failed load afterwards.
ColumnLayout {
    id: root

    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 8
        Layout.preferredHeight: 56

        Label {
            Layout.fillWidth: true
            text: qsTr("Errors")
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
        Button {
            text: qsTr("Clear")
            flat: true
            enabled: DicomController.errors.count > 0
            onClicked: DicomController.errors.clear()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.divider
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            reuseItems: true
            spacing: 8
            topMargin: 8
            bottomMargin: 8
            leftMargin: 8
            rightMargin: 8
            boundsBehavior: Flickable.StopAtBounds
            model: DicomController.errors
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: card

                required property string severity
                required property string title
                required property string detail
                required property string tag
                required property var timestamp
                required property bool preview

                readonly property color severityColor: severity === "warning" ? Theme.warning
                    : severity === "info" ? Theme.info : Theme.danger

                width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                implicitHeight: content.implicitHeight + 24
                radius: 8
                color: Theme.surfaceRaised
                border.color: Theme.border

                RowLayout {
                    id: content
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Icon {
                        Layout.alignment: Qt.AlignTop
                        glyph: card.severity === "warning" ? FontAwesome.triangleExclamation
                            : card.severity === "info" ? FontAwesome.circleInfo : FontAwesome.circleXmark
                        color: card.severityColor
                        font.pixelSize: 16
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: card.title
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                            }
                            Rectangle {
                                visible: card.preview
                                implicitWidth: chipLabel.implicitWidth + 12
                                implicitHeight: 16
                                radius: 8
                                color: "transparent"
                                border.color: Theme.accent

                                Label {
                                    id: chipLabel
                                    anchors.centerIn: parent
                                    text: qsTr("Example")
                                    color: Theme.accent
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: card.detail
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: card.tag !== "" || !card.preview

                            Label {
                                text: card.tag
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: !card.preview
                                text: Qt.formatTime(card.timestamp, "HH:mm:ss")
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            width: parent.width - 32
            visible: list.count === 0
            text: qsTr("No errors.")
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}
