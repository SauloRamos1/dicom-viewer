// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DicomViewer

// Options pane: choose how the 2D image is rendered, CPU (DCMTK windows it, Qt
// shows the 8-bit result) or GPU (raw frame uploaded once, a shader windows it).
// The choice is remembered; it falls back to the CPU on its own when the GPU
// cannot run it.
OptionsSection {
    title: qsTr("Render")

    SegmentedControl {
        Layout.fillWidth: true
        model: [qsTr("CPU"), qsTr("GPU")]
        tips: [
            qsTr("DCMTK applies window, level, invert and colormap on the CPU and the result is shown as an 8-bit image."),
            DicomController.gpuAvailable
                ? qsTr("The raw image is uploaded to the GPU once and a shader applies window, level, invert and colormap, so adjusting them is cheap.")
                : qsTr("GPU rendering is not available here, the CPU is used.")
        ]
        availability: [true, DicomController.gpuAvailable]
        currentIndex: DicomController.renderBackend
        onActivated: index => {
            DicomController.renderBackend = index
            AppSettings.renderBackend = index === DicomController.Gpu ? "gpu" : "cpu"
        }
    }
    Label {
        Layout.fillWidth: true
        visible: !DicomController.gpuAvailable
        text: qsTr("GPU rendering is unavailable on this system.")
        color: Theme.textMuted
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }
    Label {
        Layout.fillWidth: true
        visible: DicomController.hasImage && DicomController.gpuAvailable
            && DicomController.renderBackend === DicomController.Gpu && !DicomController.gpuCapable
        text: qsTr("Rendered on the CPU: the GPU path handles 8 and 16-bit grayscale with a linear window, one rescale per series and no Modality LUT.")
        color: Theme.textMuted
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }
}
