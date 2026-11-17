import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DicomViewer

// One line about the open file: name, size, bits, photometric, frames and
// transfer syntax. The details hide while no image is shown.
Rectangle {
    id: root

    component Field: Label {
        color: Theme.textMuted
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    implicitHeight: 32
    color: Theme.surface

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16

        Field {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            text: DicomController.fileName === "" ? qsTr("No file open") : DicomController.fileName
            color: DicomController.fileName === "" ? Theme.textMuted : Theme.text
            elide: Text.ElideMiddle
        }
        Field {
            visible: DicomController.hasImage
            text: qsTr("%1 x %2").arg(DicomController.columns).arg(DicomController.rows)
        }
        Field {
            visible: DicomController.hasImage
            text: qsTr("%1-bit").arg(DicomController.bitsStored)
        }
        Field {
            visible: DicomController.hasImage
            text: DicomController.photometric
        }
        Field {
            visible: DicomController.hasImage
            text: qsTr("%n frame(s)", "", DicomController.frameCount)
        }
        Field {
            visible: DicomController.hasImage
            Layout.maximumWidth: 280
            text: DicomController.transferSyntax
        }
    }
}
