import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Floating panel that fades and scales in/out when `open` changes (used by the
// Options pane). It stays visible until the closing animation has finished.
Pane {
    id: root

    property bool open: false

    visible: open || opacity > 0
    opacity: open ? 1 : 0
    scale: open ? 1 : 0.96
    transformOrigin: Item.TopRight

    Behavior on opacity {
        OpacityAnimator { duration: Theme.durationNormal; easing.type: Theme.easing }
    }
    Behavior on scale {
        ScaleAnimator { duration: Theme.durationNormal; easing.type: Theme.easing }
    }

    background: Rectangle {
        radius: 8
        color: Theme.surfaceRaised
        border.color: Theme.border
    }
}
