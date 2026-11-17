// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Row of mutually exclusive choices in one outlined box. `model` holds the
// labels, `tips` an optional tooltip per segment, and `availability` which
// segments can be chosen: an unavailable one is dimmed and ignores clicks but
// still shows its tooltip, so it can say why.
Rectangle {
    id: root

    property var model: []
    property var tips: []
    property var availability: []
    property int currentIndex: 0

    signal activated(int index)

    implicitHeight: 32
    radius: 6
    color: "transparent"
    border.width: 1
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 2

        Repeater {
            model: root.model

            AbstractButton {
                id: segment

                required property int index
                required property string modelData
                readonly property bool available: root.availability[index] !== false
                readonly property bool chosen: root.currentIndex === index

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1 // equal widths whatever the labels
                hoverEnabled: true
                text: modelData
                focusPolicy: available ? Qt.StrongFocus : Qt.NoFocus
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                Accessible.checked: chosen
                Accessible.description: root.tips[index] ?? ""
                onClicked: {
                    if (available && !chosen)
                        root.activated(index)
                }

                background: Item {
                    component Fill: Rectangle {
                        property bool shown
                        anchors.fill: parent
                        radius: 4
                        opacity: shown ? 1 : 0
                        Behavior on opacity {
                            OpacityAnimator { duration: Theme.durationFast; easing.type: Theme.easing }
                        }
                    }

                    Fill { color: Theme.hoverFill; shown: segment.hovered && segment.available && !segment.chosen }
                    Fill { color: Theme.checkedFill; shown: segment.chosen }
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.accent
                        visible: segment.visualFocus
                    }
                }

                contentItem: Label {
                    text: segment.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 13
                    font.weight: segment.chosen ? Font.DemiBold : Font.Normal
                    color: segment.chosen ? Theme.accent : segment.available ? Theme.text : Theme.textMuted
                    opacity: segment.available ? 1 : 0.5
                }

                AppToolTip {
                    visible: segment.hovered && (root.tips[segment.index] ?? "") !== ""
                    text: root.tips[segment.index] ?? ""
                    y: segment.height + 6
                    width: Math.min(implicitWidth, 260)
                }
            }
        }
    }
}
