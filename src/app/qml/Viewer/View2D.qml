// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import DicomViewer

// 2D viewport: the rendered frame on black, with orientation letters, W/L and
// frame overlays. Mouse: wheel = zoom about the cursor, left drag = pan,
// double click = fit, right drag = window width (horizontal) and level
// (vertical). R / Shift+R rotate right / left and H / V flip (the controller
// holds that transform); the frame keys and the empty state are ViewerPane's.
// Zoom is relative to the fitted image (1 = fit, 0.1 = 10%, 20 = 2000%).
// The image is drawn at its physical proportions: its box is columns x rows
// stretched by the pixel aspect (non-square pixels), then fitted. While the
// pointer rests on the image, a readout shows the pixel and its value; the
// screen-to-pixel mapping is the controller's (it knows rotation, flips and
// pixel aspect), refreshed at most once per frame.
Rectangle {
    id: root

    readonly property real minZoom: 0.1
    readonly property real maxZoom: 20
    property bool smoothing: true
    readonly property real zoom: canvas.scale
    // {x, y, text} of the pixel under the pointer, empty when there is none.
    property var readout: ({})
    readonly property bool hasReadout: readout.text !== undefined

    function updateReadout() {
        if (!DicomController.hasImage || !mouse.containsMouse || mouse.pressedButtons !== Qt.NoButton)
            readout = ({})
        else
            readout = DicomController.pixelAt(Qt.point(mouse.mouseX, mouse.mouseY), Qt.size(width, height),
                                              canvas.scale, Qt.point(canvas.x, canvas.y))
    }

    // One update per frame, whatever moved (pointer, zoom, pan, transform, frame).
    function scheduleReadout() {
        if (!readoutTimer.running)
            readoutTimer.start()
    }

    function fit() {
        fitAnimation.stop()
        canvas.scale = 1
        canvas.x = 0
        canvas.y = 0
    }

    function fitAnimated() {
        fitAnimation.restart()
    }

    // Zoom about the viewport centre (the Options pane's zoom control).
    function setZoom(value) {
        zoomAt(value / canvas.scale, Qt.point(width / 2, height / 2))
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

    Keys.onPressed: event => {
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
            return
        else if (event.key === Qt.Key_R && (event.modifiers & Qt.ShiftModifier))
            DicomController.rotateLeft()
        else if (event.key === Qt.Key_R)
            DicomController.rotateRight()
        else if (event.key === Qt.Key_H)
            DicomController.flipHorizontal = !DicomController.flipHorizontal
        else if (event.key === Qt.Key_V)
            DicomController.flipVertical = !DicomController.flipVertical
        else
            return
        event.accepted = true
    }

    Connections {
        target: DicomController
        function onLoaded() {
            root.fit()
        }
        function onHasImageChanged() { root.scheduleReadout() }
        function onInfoChanged() { root.scheduleReadout() }
        function onFrameChanged() { root.scheduleReadout() }
        function onViewTransformChanged() { root.scheduleReadout() }
    }
    Connections {
        target: canvas
        function onScaleChanged() { root.scheduleReadout() }
        function onXChanged() { root.scheduleReadout() }
        function onYChanged() { root.scheduleReadout() }
    }
    onWidthChanged: scheduleReadout()
    onHeightChanged: scheduleReadout()
    Timer {
        id: readoutTimer

        interval: 16
        onTriggered: root.updateReadout()
    }

    // GPU path timing: the controller closes a sample at the next frame swap.
    Connections {
        target: root.Window.window
        function onFrameSwapped() {
            DicomController.reportFrameSwapped()
        }
    }

    // The shader path needs a hardware scene graph.
    readonly property int graphicsApi: GraphicsInfo.api
    function checkGraphicsApi() {
        if (graphicsApi === GraphicsInfo.Software || graphicsApi === GraphicsInfo.OpenVG
                || graphicsApi === GraphicsInfo.Null)
            DicomController.reportGpuUnavailable("Qt Quick is not using a GPU scene graph (API " + graphicsApi + ")")
    }
    onGraphicsApiChanged: checkGraphicsApi()
    Component.onCompleted: checkGraphicsApi()

    Item {
        id: canvas

        width: root.width
        height: root.height
        visible: DicomController.hasImage

        // Flip mirrors the screen, the rotator turns the image inside it. A
        // quarter turn swaps the rotator's size so the turned image still
        // fits the viewport.
        Item {
            id: flipper

            anchors.fill: parent
            transform: Scale {
                origin.x: flipper.width / 2
                origin.y: flipper.height / 2
                xScale: DicomController.flipHorizontal ? -1 : 1
                yScale: DicomController.flipVertical ? -1 : 1
            }

            Item {
                readonly property bool sideways: DicomController.viewRotation % 2 === 1

                width: sideways ? parent.height : parent.width
                height: sideways ? parent.width : parent.height
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2
                rotation: DicomController.viewRotation * 90

                // The image box: the displayed aspect (pixel aspect included)
                // fitted into the rotator; the content stretches to fill it.
                Item {
                    readonly property real displayAspect: DicomController.rows > 0
                        ? DicomController.columns * DicomController.pixelAspect / DicomController.rows : 1

                    anchors.centerIn: parent
                    width: Math.min(parent.width, parent.height * displayAspect)
                    height: width / displayAspect

                    // The content differs by render backend, the transform around it does not.
                    // Synchronous like GpuImage's: gpuActive flips right after a new
                    // revision, and an async load would leave a blank frame in between.
                    Image {
                        anchors.fill: parent
                        visible: !DicomController.gpuActive
                        source: DicomController.hasImage ? "image://dicom/" + DicomController.imageRevision : ""
                        fillMode: Image.Stretch
                        smooth: root.smoothing
                        cache: false
                    }
                    GpuImage {
                        anchors.fill: parent
                        visible: DicomController.gpuActive
                        smoothing: root.smoothing
                    }
                }
            }
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
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: pressedButtons & Qt.LeftButton ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onContainsMouseChanged: root.scheduleReadout()
        onPressedButtonsChanged: root.scheduleReadout()

        onPressed: event => {
            root.forceActiveFocus()
            last = Qt.point(event.x, event.y)
        }
        onPositionChanged: event => {
            root.scheduleReadout()
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
        id: frameLabel

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        text: qsTr("Frame %1/%2").arg(DicomController.frame + 1).arg(DicomController.frameCount)
        font.pixelSize: 12
        opacity: 0.8
    }
    // Pixel under the pointer: position and modality value (HU for CT).
    Overlay {
        anchors.right: parent.right
        anchors.bottom: frameLabel.top
        anchors.rightMargin: 8
        text: root.hasReadout ? qsTr("x, y: %1, %2  value: %3").arg(root.readout.x).arg(root.readout.y).arg(root.readout.text) : ""
        font.pixelSize: 12
        opacity: 0.8
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
