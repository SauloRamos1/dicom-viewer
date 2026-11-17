// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DicomViewer

// Options pane, shown only in the 3D view: the controls of the kind of 3D the
// open file gets. A single image is a heightmap (depth, wireframe); a series or
// multi-frame file is a volume (threshold, opacity, sample steps). Controls that
// do not apply to the current kind are not shown. The values are remembered.
OptionsSection {
    id: root

    required property ViewerPane viewer

    component Hint: Label {
        Layout.fillWidth: true
        color: Theme.textMuted
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }

    title: qsTr("3D")
    actionText: qsTr("Reset 3D")
    onActionTriggered: {
        AppSettings.resetView3D()
        viewer.wireframe = false
        viewer.view3D.resetCamera()
    }

    // ---- Heightmap: a single image ----------------------------------------
    SliderField {
        visible: !DicomController.hasVolume
        label: qsTr("Depth")
        from: AppSettings.minDepthScale
        to: 1
        stepSize: 0.01
        displayScale: 100
        suffix: "%"
        value: AppSettings.depthScale
        onMoved: v => AppSettings.depthScale = v
    }
    SwitchRow {
        visible: !DicomController.hasVolume
        text: qsTr("Wireframe")
        checked: root.viewer.wireframe
        onToggled: root.viewer.wireframe = checked
    }
    Hint {
        visible: !DicomController.hasVolume && DicomController.hasImage
        text: qsTr("A single image is shown as a heightmap. For a volume, open a multi-frame file or a series folder.")
    }

    // ---- Volume: a series or a multi-frame file ---------------------------
    SliderField {
        visible: DicomController.hasVolume
        label: qsTr("Threshold")
        from: 0
        to: 1
        stepSize: 0.01
        displayScale: 100
        suffix: "%"
        value: AppSettings.threshold
        onMoved: v => AppSettings.threshold = v
    }
    SliderField {
        visible: DicomController.hasVolume
        label: qsTr("Opacity")
        from: AppSettings.minOpacity
        to: 1
        stepSize: 0.01
        displayScale: 100
        suffix: "%"
        value: AppSettings.opacity
        onMoved: v => AppSettings.opacity = v
    }
    SliderField {
        visible: DicomController.hasVolume
        label: qsTr("Steps")
        from: AppSettings.minSampleSteps
        to: AppSettings.maxSampleSteps
        stepSize: 16
        value: AppSettings.sampleSteps
        onMoved: v => AppSettings.sampleSteps = v
    }
    Hint {
        visible: DicomController.hasVolume && DicomController.volumeDownsample > 1
        text: qsTr("Volume downsampled by %1").arg(DicomController.volumeDownsample)
    }

    // ---- Both -------------------------------------------------------------
    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Reset camera")
        flat: true
        font.pixelSize: 12
        implicitHeight: 32
        topInset: 0
        bottomInset: 0
        topPadding: 0
        bottomPadding: 0
        onClicked: root.viewer.view3D.resetCamera()
    }
    Hint {
        text: qsTr("Drag to orbit, wheel to zoom, Ctrl+drag to pan, double-click to reset the camera. Right-drag changes window and level.")
    }
    Hint {
        text: qsTr("Zoom, transform and render backend apply to the 2D view.")
    }
}
