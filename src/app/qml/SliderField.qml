import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Label, slider and an editable number for one value. `value`, `from` and `to`
// are in the value's own units; the number shows value * displayScale (zoom 1.5
// as 150). The slider is linear, or logarithmic for ranges spanning decades.
// Changes made by the user are reported through moved(); the owner writes them
// back, so the control never fights the value it shows (the slider clamps
// values outside its range, the number shows them as they are).
RowLayout {
    id: root

    property string label
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 1
    property real displayScale: 1
    property string suffix
    property bool logarithmic: false

    signal moved(real value)

    function position(v) {
        if (!(to > from))
            return 0
        const t = logarithmic ? Math.log(Math.max(v, from) / from) / Math.log(to / from)
                              : (v - from) / (to - from)
        return Math.min(1, Math.max(0, t))
    }
    function valueAt(t) {
        return logarithmic ? from * Math.pow(to / from, t)
                           : Math.round((from + t * (to - from)) / stepSize) * stepSize
    }
    function format(v) {
        return Math.round(v * displayScale) + suffix
    }

    Layout.fillWidth: true
    Layout.preferredHeight: 32
    spacing: 8

    Label {
        visible: root.label !== ""
        Layout.preferredWidth: 80
        text: root.label
        elide: Text.ElideRight
        color: root.enabled ? Theme.text : Theme.textMuted
    }
    Slider {
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        stepSize: root.logarithmic ? 0.02 : root.stepSize / Math.max(1, root.to - root.from)
        value: root.position(root.value)
        Accessible.name: root.label
        onMoved: root.moved(root.valueAt(value))
    }
    TextField {
        id: field

        property bool edited: false

        Layout.preferredWidth: 64
        Layout.preferredHeight: 32
        horizontalAlignment: Text.AlignRight
        font.pixelSize: 13
        topPadding: 4
        bottomPadding: 4
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
        Accessible.name: root.label

        // Follows the value except while the user is typing.
        Binding on text {
            when: !field.edited
            value: root.format(root.value)
        }
        onTextEdited: edited = true
        onEditingFinished: {
            const typed = Number(text.replace(",", ".").replace(/[^0-9.\-]/g, ""))
            if (edited && text.trim() !== "" && !isNaN(typed))
                root.moved(Math.min(root.to, Math.max(root.from, typed / root.displayScale)))
            edited = false
            text = root.format(root.value)
        }
    }
}
