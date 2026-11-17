import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DicomViewer

// Options pane: window preset, width and level. Color images have no window, so
// the controls are disabled with a hint.
OptionsSection {
    id: root

    readonly property var presetNames: ["default", "minmax", "brain", "lung", "bone", "abdomen"]
    // Set while a preset is being applied, so its own window change is not
    // mistaken for a manual one.
    property bool applying: false

    function showPreset(index) {
        presets.currentIndex = index
    }
    function applyPreset(index) {
        applying = true
        DicomController.applyPreset(presetNames[index])
        applying = false
        showPreset(index)
    }

    title: qsTr("Window / Level")
    actionText: qsTr("Reset")
    actionEnabled: DicomController.windowable
    onActionTriggered: {
        DicomController.resetWindow()
        showPreset(0)
    }

    Connections {
        target: DicomController
        function onLoaded() {
            root.showPreset(0)
        }
        function onWindowChanged() {
            if (!root.applying)
                root.showPreset(-1)
        }
    }

    LabeledCombo {
        id: presets
        enabled: DicomController.windowable
        label: qsTr("Preset")
        currentIndex: 0
        displayText: currentIndex < 0 ? qsTr("Custom") : currentText
        model: [qsTr("Default from file"), qsTr("Min-Max"), qsTr("CT Brain"), qsTr("CT Lung"),
                qsTr("CT Bone"), qsTr("CT Abdomen")]
        onActivated: index => root.applyPreset(index)
    }
    SliderField {
        enabled: DicomController.windowable
        label: qsTr("Width")
        from: 1
        to: Math.max(2, DicomController.valueMax - DicomController.valueMin + 1)
        value: DicomController.windowWidth
        onMoved: v => DicomController.windowWidth = v
    }
    SliderField {
        enabled: DicomController.windowable
        label: qsTr("Level")
        from: DicomController.valueMin
        to: Math.max(DicomController.valueMax, DicomController.valueMin + 1)
        value: DicomController.windowCenter
        onMoved: v => DicomController.windowCenter = v
    }
    Label {
        Layout.fillWidth: true
        visible: DicomController.hasImage && !DicomController.windowable
        text: qsTr("Window and colormap apply to grayscale images only.")
        color: Theme.textMuted
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }
}
