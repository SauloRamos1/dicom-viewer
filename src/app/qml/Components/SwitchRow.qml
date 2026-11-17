// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Label on the left, switch on the right; clicking the label toggles too.
RowLayout {
    id: root

    property string text
    property alias checked: toggle.checked

    signal toggled

    Layout.fillWidth: true
    Layout.preferredHeight: 32

    Label {
        Layout.fillWidth: true
        text: root.text
        elide: Text.ElideRight
        color: root.enabled ? Theme.text : Theme.textMuted

        TapHandler {
            enabled: root.enabled
            onTapped: {
                toggle.toggle()
                root.toggled()
            }
        }
    }
    Switch {
        id: toggle
        rightPadding: 0
        Accessible.name: root.text
        onToggled: root.toggled()
    }
}
