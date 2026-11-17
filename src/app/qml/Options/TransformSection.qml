// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DicomViewer

// Options pane: zoom, rotate, flip and reset of the 2D view.
OptionsSection {
    id: root

    required property View2D view

    component PaneButton: IconButton {
        implicitWidth: 32
        implicitHeight: 32
    }

    title: qsTr("Transform")
    enabled: DicomController.hasImage
    actionText: qsTr("Reset view")
    onActionTriggered: {
        DicomController.resetTransform()
        view.fitAnimated()
    }

    SliderField {
        label: qsTr("Zoom")
        from: root.view.minZoom
        to: root.view.maxZoom
        value: root.view.zoom
        displayScale: 100
        suffix: "%"
        logarithmic: true
        onMoved: v => root.view.setZoom(v)
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 4

        PaneButton {
            glyph: FontAwesome.rotateLeft
            tip: qsTr("Rotate left (Shift+R)")
            onClicked: DicomController.rotateLeft()
        }
        PaneButton {
            glyph: FontAwesome.rotateRight
            tip: qsTr("Rotate right (R)")
            onClicked: DicomController.rotateRight()
        }
        PaneButton {
            glyph: FontAwesome.leftRight
            tip: qsTr("Flip horizontal (H)")
            checkable: true
            checked: DicomController.flipHorizontal
            onToggled: DicomController.flipHorizontal = checked
        }
        PaneButton {
            glyph: FontAwesome.upDown
            tip: qsTr("Flip vertical (V)")
            checkable: true
            checked: DicomController.flipVertical
            onToggled: DicomController.flipVertical = checked
        }
        Item { Layout.fillWidth: true }
        Button {
            text: qsTr("Fit")
            flat: true
            font.pixelSize: 12
            implicitHeight: 32
            topInset: 0
            bottomInset: 0
            topPadding: 0
            bottomPadding: 0
            onClicked: root.view.fitAnimated()
        }
    }
}
