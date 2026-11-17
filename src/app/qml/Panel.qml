// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Collapsible SplitView side panel. `open` animates the width between 0 and
// the last width the user dragged it to (`restoreWidth`) while the content
// fades. The content keeps `restoreWidth` and stays anchored to the left edge
// while the width animates, so the pane only reveals it instead of relaying it
// out. The pane only turns invisible once it is fully collapsed.
Pane {
    id: root

    // Content children are placed in the fixed-width body below.
    default property alias content: body.data

    property bool open: true
    property real restoreWidth: 300
    // Animated by the open/closed transition; drives SplitView.preferredWidth.
    property real animWidth: restoreWidth
    readonly property bool animating: widthTransition.running

    // The SplitView overwrites its own preferredWidth while dragging, so the
    // animated value is pushed through a Binding rather than a plain binding.
    Binding {
        target: root.SplitView
        property: "preferredWidth"
        value: root.animWidth
    }
    SplitView.minimumWidth: Math.min(180, animWidth)
    visible: open || animWidth > 0
    padding: 0
    clip: width < restoreWidth || animating

    Connections {
        target: root.SplitView.view
        function onResizingChanged() {
            if (!root.SplitView.view.resizing && root.open)
                root.restoreWidth = root.width
        }
    }

    background: Rectangle {
        color: Theme.surface
    }
    contentItem: Item {
        Item {
            id: body
            // Follows the pane while the user drags it; fixed while animating.
            width: root.animating ? root.restoreWidth : parent.width
            height: parent.height
            opacity: root.open ? 1 : 0
            Behavior on opacity {
                OpacityAnimator { duration: Theme.durationFast; easing.type: Theme.easing }
            }
        }
    }

    states: State {
        name: "closed"
        when: !root.open
        PropertyChanges {
            root.animWidth: 0
        }
    }
    transitions: Transition {
        id: widthTransition
        NumberAnimation {
            property: "animWidth"
            duration: Theme.durationSlow
            easing.type: Theme.easing
        }
    }
}
