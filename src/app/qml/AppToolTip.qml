// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls

// Tooltip in the toolbar style, for text that is not a button hint.
ToolTip {
    id: root

    delay: 400
    leftPadding: 12
    rightPadding: 12
    topPadding: 6
    bottomPadding: 6
    margins: 8

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
        radius: 4
        color: Theme.tooltipBg
    }
    contentItem: Label {
        text: root.text
        color: Theme.tooltipText
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }
}
