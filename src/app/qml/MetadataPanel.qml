import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import DicomViewer

// Tag list of the open file, grouped by category, with a filter field.
ColumnLayout {
    id: root

    spacing: 0

    TextField {
        Layout.fillWidth: true
        Layout.margins: 8
        Layout.preferredHeight: 40
        topPadding: 8
        bottomPadding: 8
        placeholderText: qsTr("Filter tags...")
        enabled: DicomController.fileName !== ""
        selectByMouse: true
        Material.containerStyle: Material.Outlined
        onTextChanged: DicomController.metadata.filterText = text
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Theme.divider
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            model: DicomController.metadata
            ScrollBar.vertical: ScrollBar {}

            section.property: "category"
            section.criteria: ViewSection.FullString
            section.delegate: Rectangle {
                required property string section

                width: ListView.view.width
                height: 32
                color: Theme.surfaceRaised

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.divider
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 8
                    text: parent.section === "patient" ? qsTr("Patient")
                        : parent.section === "study" ? qsTr("Study / Series")
                        : parent.section === "image" ? qsTr("Image")
                        : qsTr("Other")
                    color: Theme.accent
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }

            delegate: Item {
                id: row

                required property string tag
                required property string name
                required property string vr
                required property string value
                required property int depth

                width: ListView.view.width
                height: 32

                HoverHandler {
                    id: hover
                }
                Rectangle {
                    anchors.fill: parent
                    color: Theme.hoverFill
                    visible: hover.hovered
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8 + row.depth * 12
                    anchors.rightMargin: 8
                    spacing: 8

                    Label {
                        Layout.preferredWidth: 80
                        text: row.tag
                        color: Theme.textMuted
                        font.pixelSize: 11
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 100
                        text: row.name
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.preferredWidth: 24
                        text: row.vr
                        color: Theme.textMuted
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        id: valueLabel
                        Layout.fillWidth: true
                        Layout.preferredWidth: 100
                        text: row.value
                        color: Theme.textMuted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                AppToolTip {
                    visible: hover.hovered && valueLabel.truncated
                    text: row.value
                    x: row.width - width - 8
                    y: row.height + 4
                }
            }
        }

        Label {
            anchors.centerIn: parent
            width: parent.width - 32
            visible: list.count === 0
            text: DicomController.fileName === "" ? qsTr("Open a DICOM file to see its tags.")
                : qsTr("No tags match the filter.")
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}
