// SPDX-License-Identifier: GPL-3.0-only
pragma Singleton
import QtQuick

// Font Awesome Free (Solid) font and the glyphs used by the UI.
QtObject {
    id: root

    readonly property string family: loader.name

    readonly property string file: ""
    readonly property string folderOpen: ""
    readonly property string sliders: ""
    readonly property string circleInfo: ""
    readonly property string circleXmark: ""
    readonly property string cube: ""
    readonly property string triangleExclamation: ""
    readonly property string language: ""
    readonly property string moon: ""
    readonly property string sun: ""
    readonly property string xmark: ""
    readonly property string rotateLeft: ""
    readonly property string rotateRight: ""
    readonly property string leftRight: ""
    readonly property string upDown: ""
    readonly property string expand: ""
    readonly property string chevronDown: ""

    property FontLoader loader: FontLoader {
        source: "fonts/fa-solid-900.otf"
    }
}
