// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Label on the left, ComboBox filling the rest of the row.
RowLayout {
    id: root

    property string label
    property alias model: combo.model
    property alias currentIndex: combo.currentIndex
    readonly property alias currentText: combo.currentText
    property alias displayText: combo.displayText

    signal activated(int index)

    Layout.fillWidth: true
    Layout.preferredHeight: 32
    spacing: 8

    Label {
        Layout.preferredWidth: 80
        text: root.label
        elide: Text.ElideRight
        color: root.enabled ? Theme.text : Theme.textMuted
    }
    ComboBox {
        id: combo
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        font.pixelSize: 13
        Accessible.name: root.label
        onActivated: index => root.activated(index)
    }
}
