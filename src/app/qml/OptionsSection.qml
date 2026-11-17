import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Titled group of controls in the Options pane, with an optional text action on
// the title row (Reset). Children go below the title, 8 px apart.
ColumnLayout {
    id: root

    property string title
    property string actionText
    property bool actionEnabled: true
    default property alias content: body.data

    signal actionTriggered

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 24

        Label {
            Layout.fillWidth: true
            text: root.title.toUpperCase()
            elide: Text.ElideRight
            color: Theme.textMuted
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0.8
        }
        Button {
            visible: root.actionText !== ""
            enabled: root.actionEnabled
            flat: true
            text: root.actionText
            font.pixelSize: 12
            implicitHeight: 24
            topInset: 0
            bottomInset: 0
            topPadding: 0
            bottomPadding: 0
            leftPadding: 8
            rightPadding: 8
            onClicked: root.actionTriggered()
        }
    }

    ColumnLayout {
        id: body
        Layout.fillWidth: true
        spacing: 8
    }
}
