// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import DicomViewer

// Options pane: invert, colormap (grayscale only) and smooth interpolation
// (2D only). Invert and colormap drive the 3D view too.
OptionsSection {
    title: qsTr("Display")

    SwitchRow {
        enabled: DicomController.hasImage
        text: qsTr("Invert")
        checked: DicomController.invert
        onToggled: DicomController.invert = checked
    }
    LabeledCombo {
        enabled: DicomController.windowable
        label: qsTr("Colormap")
        currentIndex: DicomController.colormap
        model: [qsTr("Gray"), qsTr("Bone"), qsTr("Hot"), qsTr("Jet")]
        onActivated: index => DicomController.colormap = index
    }
    SwitchRow {
        visible: !DicomController.view3D
        text: qsTr("Smooth interpolation")
        checked: AppSettings.smoothing
        onToggled: AppSettings.smoothing = checked
    }
}
