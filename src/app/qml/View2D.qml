import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import DicomViewer

// 2D viewport: the rendered frame on black, with orientation letters, W/L and
// frame overlays. Mouse: wheel = zoom about the cursor, left drag = pan,
// double click = fit, right drag = window width (horizontal) and level
// (vertical). Arrow keys / Page Up / Page Down step through frames. Zoom is
// relative to the fitted image (1 = fit, 0.1 = 10%, 20 = 2000%).
Rectangle {
    id: root

    readonly property real minZoom: 0.1
    readonly property real maxZoom: 20
    property bool smoothing: true

    function fit() {
        fitAnimation.stop()
        canvas.scale = 1
        canvas.x = 0
        canvas.y = 0
    }

    function fitAnimated() {
        fitAnimation.restart()
    }

    function zoomAt(factor, cursor) {
        fitAnimation.stop()
        const next = Math.min(maxZoom, Math.max(minZoom, canvas.scale * factor))
        const k = next / canvas.scale
        const cx = width / 2
        const cy = height / 2
        canvas.x = cursor.x - cx - (cursor.x - canvas.x - cx) * k
        canvas.y = cursor.y - cy - (cursor.y - canvas.y - cy) * k
        canvas.scale = next
    }

    color: Theme.viewportBg
    clip: true
    focus: true

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Up || event.key === Qt.Key_PageUp)
            DicomController.frame--
        else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down || event.key === Qt.Key_PageDown)
            DicomController.frame++
        else
            return
        event.accepted = true
    }

    Connections {
        target: DicomController
        function onLoaded() {
            root.fit()
        }
    }

    Item {
        id: canvas

        width: root.width
        height: root.height
        visible: DicomController.hasImage

        Image {
            anchors.fill: parent
            source: DicomController.hasImage ? "image://dicom/" + DicomController.imageRevision : ""
            fillMode: Image.PreserveAspectFit
            smooth: root.smoothing
            cache: false
            asynchronous: true
        }
    }

    ParallelAnimation {
        id: fitAnimation

        ScaleAnimator {
            target: canvas; to: 1
            duration: Theme.durationNormal; easing.type: Theme.easing
        }
        XAnimator {
            target: canvas; to: 0
            duration: Theme.durationNormal; easing.type: Theme.easing
        }
        YAnimator {
            target: canvas; to: 0
            duration: Theme.durationNormal; easing.type: Theme.easing
        }
    }

    MouseArea {
        id: mouse

        property point last

        anchors.fill: parent
        enabled: DicomController.hasImage
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: pressedButtons & Qt.LeftButton ? Qt.ClosedHandCursor : Qt.ArrowCursor

        onPressed: event => {
            root.forceActiveFocus()
            last = Qt.point(event.x, event.y)
        }
        onPositionChanged: event => {
            const dx = event.x - last.x
            const dy = event.y - last.y
            last = Qt.point(event.x, event.y)
            if (pressedButtons & Qt.LeftButton) {
                fitAnimation.stop()
                canvas.x += dx
                canvas.y += dy
            } else if ((pressedButtons & Qt.RightButton) && DicomController.windowable) {
                // About 200 pixels sweep the whole current width.
                const step = Math.max(1, DicomController.windowWidth / 200)
                DicomController.windowWidth += dx * step
                DicomController.windowCenter += dy * step
            }
        }
        onDoubleClicked: root.fitAnimated()
        onWheel: event => {
            const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.pixelDelta.y * 4
            root.zoomAt(Math.pow(1.1, delta / 120), Qt.point(event.x, event.y))
        }
    }

    // ---- Overlays ---------------------------------------------------------
    component Overlay: Label {
        color: Theme.overlayText
        style: Text.Outline
        styleColor: Theme.viewportBg
        visible: DicomController.hasImage && text !== ""
    }

    Overlay {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 8
        text: DicomController.orientationTop
        font.pixelSize: 16
        font.weight: Font.DemiBold
    }
    Overlay {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        text: DicomController.orientationBottom
        font.pixelSize: 16
        font.weight: Font.DemiBold
    }
    Overlay {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 8
        text: DicomController.orientationLeft
        font.pixelSize: 16
        font.weight: Font.DemiBold
    }
    Overlay {
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 8
        text: DicomController.orientationRight
        font.pixelSize: 16
        font.weight: Font.DemiBold
    }
    Overlay {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        visible: DicomController.hasImage && DicomController.windowable
        text: qsTr("W: %1  L: %2")
            .arg(Math.round(DicomController.windowWidth))
            .arg(Math.round(DicomController.windowCenter))
        font.pixelSize: 12
        opacity: 0.8
    }
    Overlay {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        text: qsTr("Frame %1/%2").arg(DicomController.frame + 1).arg(DicomController.frameCount)
        font.pixelSize: 12
        opacity: 0.8
    }

    // ---- Empty state and busy indicator -----------------------------------
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
