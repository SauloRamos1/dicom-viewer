import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DicomViewer

// Floating Options pane over the viewer. The header drags it; its place is kept
// as fractions (fx, fy) of the room it has inside the viewer, so it stays put
// when the pane is closed and reopened and follows window resizes, always
// fully inside. A body taller than the viewer scrolls.
FloatingPane {
    id: root

    required property ViewerPane viewer

    readonly property int margin: 16
    property real fx: 1
    property real fy: 0

    signal closeRequested
    signal positionCommitted

    // True when the keyboard focus is on the pane or something in it.
    function containsFocus() {
        for (let item = Window.window?.activeFocusItem; item; item = item.parent) {
            if (item === root)
                return true
        }
        return false
    }

    function moveTo(px, py) {
        const roomX = parent.width - width - 2 * margin
        const roomY = parent.height - height - 2 * margin
        if (roomX > 0)
            fx = Math.min(1, Math.max(0, (px - margin) / roomX))
        if (roomY > 0)
            fy = Math.min(1, Math.max(0, (py - margin) / roomY))
    }

    width: Math.min(300, parent.width - 2 * margin)
    height: Math.min(implicitHeight, parent.height - 2 * margin)
    x: margin + fx * Math.max(0, parent.width - width - 2 * margin)
    y: margin + fy * Math.max(0, parent.height - height - 2 * margin)
    padding: 0
    Keys.onEscapePressed: root.closeRequested()

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 40

            DragHandler {
                property real startX
                property real startY

                target: null
                cursorShape: Qt.SizeAllCursor
                onActiveChanged: {
                    if (active) {
                        startX = root.x
                        startY = root.y
                    } else {
                        root.positionCommitted()
                    }
                }
                onActiveTranslationChanged: {
                    if (active)
                        root.moveTo(startX + activeTranslation.x, startY + activeTranslation.y)
                }
            }

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Options")
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            IconButton {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                glyph: FontAwesome.xmark
                tip: qsTr("Close")
                implicitWidth: 32
                implicitHeight: 32
                onClicked: root.closeRequested()
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: column.implicitHeight + 32
            contentHeight: column.implicitHeight + 32
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: column
                x: 16
                y: 16
                width: flick.width - 32
                spacing: 24

                WindowLevelSection {
                    Layout.fillWidth: true
                }
                DisplaySection {
                    Layout.fillWidth: true
                }
                ThreeDSection {
                    Layout.fillWidth: true
                    visible: root.viewer.shows3D
                    viewer: root.viewer
                }
                TransformSection {
                    Layout.fillWidth: true
                    visible: !root.viewer.shows3D
                    view: root.viewer.view2D
                }
                OptionsSection {
                    Layout.fillWidth: true
                    // In 3D only a heightmap follows the frame; a volume shows all of them.
                    visible: DicomController.frameCount > 1 && !(root.viewer.shows3D && DicomController.hasVolume)
                    title: qsTr("Frame")

                    SliderField {
                        from: 1
                        to: Math.max(2, DicomController.frameCount)
                        value: DicomController.frame + 1
                        onMoved: v => DicomController.frame = v - 1
                    }
                }
                RenderSection {
                    Layout.fillWidth: true
                    visible: !root.viewer.shows3D
                }
                Label {
                    Layout.fillWidth: true
                    visible: !root.viewer.shows3D && DicomController.hasImage && DicomController.lastRenderMs > 0
                    text: qsTr("Render: %1 ms (%2), average %3 ms")
                        .arg(DicomController.lastRenderMs.toFixed(1))
                        .arg(DicomController.lastRenderBackend === DicomController.Gpu ? qsTr("GPU") : qsTr("CPU"))
                        .arg(DicomController.averageRenderMs.toFixed(1))
                    color: Theme.textMuted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
