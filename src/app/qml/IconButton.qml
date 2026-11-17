import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Toolbar button: Font Awesome glyph, optional count badge, optional shortcut,
// and a tooltip anchored right below it ("Open file (Ctrl+O)"). Plain by
// default; set `checkable: true` for a toggle.
ToolButton {
    id: root

    property string glyph
    property string tip
    property string shortcut
    property int badgeCount: 0
    // Hides the tooltip while the button's own popup (menu) is open.
    property bool tipSuppressed: false

    implicitWidth: 40
    implicitHeight: 40
    padding: 0
    hoverEnabled: true
    Accessible.name: tip
    Accessible.description: shortcutHint.nativeText

    Shortcut {
        id: shortcutHint
        sequence: root.shortcut
        enabled: root.shortcut !== "" && root.enabled
        onActivated: root.click()
    }

    // State fades animate opacity only, so a theme switch (which changes the
    // fill colors themselves) is never delayed by a second animation.
    background: Item {
        component Fill: Rectangle {
            property bool shown
            anchors.fill: parent
            radius: 8
            opacity: shown ? 1 : 0
            Behavior on opacity {
                OpacityAnimator { duration: Theme.durationFast; easing.type: Theme.easing }
            }
        }

        Fill { color: Theme.hoverFill; shown: root.hovered }
        Fill { color: Theme.checkedFill; shown: root.checked }
        Fill { color: Theme.pressedFill; shown: root.down }
        Rectangle {
            anchors.fill: parent
            radius: 8
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
            visible: root.visualFocus
        }
    }

    contentItem: Item {
        Icon {
            property real checkedAmount: root.checked ? 1 : 0

            anchors.centerIn: parent
            glyph: root.glyph
            color: !root.enabled ? Theme.textMuted
                : Qt.tint(Theme.text, Qt.alpha(Theme.accent, checkedAmount))
            opacity: root.enabled ? 1 : 0.5
            Behavior on checkedAmount {
                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
            }
        }

        Rectangle {
            id: badge
            visible: root.badgeCount > 0
            x: parent.width - width - 2
            y: 2
            width: Math.max(height, badgeLabel.implicitWidth + 8)
            height: 16
            radius: height / 2
            color: Theme.danger

            Label {
                id: badgeLabel
                anchors.centerIn: parent
                text: root.badgeCount
                color: Theme.dangerText
                font.pixelSize: 10
                font.weight: Font.Bold
            }
            Connections {
                target: root
                function onBadgeCountChanged() {
                    if (root.badgeCount > 0)
                        pop.restart()
                }
            }
            SequentialAnimation {
                id: pop
                ScaleAnimator {
                    target: badge; to: 1.3
                    duration: Theme.durationFast / 2; easing.type: Theme.easing
                }
                ScaleAnimator {
                    target: badge; to: 1
                    duration: Theme.durationFast; easing.type: Easing.InOutQuad
                }
            }
        }
    }

    ToolTip {
        id: tooltip
        readonly property real slide: 4

        visible: root.hovered && !root.down && !root.tipSuppressed
        delay: 400
        text: root.shortcut === "" ? root.tip : qsTr("%1 (%2)").arg(root.tip).arg(shortcutHint.nativeText)
        x: (root.width - width) / 2
        y: root.height + 6
        leftPadding: 12
        rightPadding: 12
        topPadding: 6
        bottomPadding: 6
        margins: 8 // keeps the tooltip inside the window for the right-most buttons

        enter: Transition {
            OpacityAnimator {
                from: 0; to: 1
                duration: Theme.durationFast; easing.type: Theme.easing
            }
            YAnimator {
                from: root.height + 6 - tooltip.slide; to: root.height + 6
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
            radius: 4
            color: Theme.tooltipBg
        }
        contentItem: Label {
            text: tooltip.text
            color: Theme.tooltipText
            font.pixelSize: 12
        }
    }
}
