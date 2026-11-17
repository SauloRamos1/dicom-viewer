import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import DicomViewer

// The viewport: the 2D view or the 3D view (the TopBar's 3D button decides),
// with the empty state and the busy indicator shared by both. Both views stay
// alive and cross-fade, so the 2D zoom and pan survive a round trip through 3D.
// Arrow keys and Page Up / Page Down step through frames in either view.
FocusScope {
    id: root

    property bool smoothing: true
    // Wireframe of the 3D heightmap (not remembered, like the camera).
    property bool wireframe: false
    readonly property bool shows3D: DicomController.view3D
    readonly property alias view2D: view2D
    readonly property alias view3D: view3D

    // Gives the keyboard focus to the view on screen.
    function focusView() {
        (shows3D ? view3D : view2D).forceActiveFocus()
    }

    onShows3DChanged: focusView()
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Up || event.key === Qt.Key_PageUp)
            DicomController.frame--
        else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down || event.key === Qt.Key_PageDown)
            DicomController.frame++
        else
            return
        event.accepted = true
    }

    View2D {
        id: view2D

        anchors.fill: parent
        focus: !root.shows3D
        enabled: !root.shows3D
        visible: opacity > 0
        opacity: root.shows3D ? 0 : 1
        smoothing: root.smoothing

        Behavior on opacity {
            OpacityAnimator { duration: Theme.durationNormal; easing.type: Theme.easing }
        }
    }
    Viewport3D {
        id: view3D

        anchors.fill: parent
        focus: root.shows3D
        enabled: root.shows3D
        visible: opacity > 0
        opacity: root.shows3D ? 1 : 0
        wireframe: root.wireframe

        Behavior on opacity {
            OpacityAnimator { duration: Theme.durationNormal; easing.type: Theme.easing }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        visible: !DicomController.hasImage && !DicomController.busy
        width: Math.min(parent.width - 32, 360)

        Icon {
            Layout.alignment: Qt.AlignHCenter
            glyph: FontAwesome.folderOpen
            color: Theme.overlayText
            font.pixelSize: 32
            opacity: 0.5
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Open a DICOM file (Ctrl+O) or drop it here")
            color: Theme.overlayText
            opacity: 0.7
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: DicomController.busy
        Material.accent: Theme.accent
    }
}
