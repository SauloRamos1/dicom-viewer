import QtQuick
import QtQuick.Controls

// A single Font Awesome glyph (see FontAwesome.qml).
Label {
    property string glyph

    text: glyph
    color: Theme.text
    font.family: FontAwesome.family
    font.pixelSize: 18
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    Accessible.ignored: true
}
