pragma Singleton
import QtQuick
import QtQuick.Controls.Material
import DicomViewer

// Semantic colour and motion tokens. Every QML file takes its colours and
// durations from here; nothing else hardcodes either.
//
// Palette sources:
//   Dark  - OHIF Viewer (Tailwind config): deep navy surfaces #090c29 / #041c4a,
//           primary cyan-blue #5acce6, active blue #348cfd, muted aqua #7bb2ce.
//           The viewport stays pure black (medical imaging convention).
//   Light - Tailwind CSS slate scale (50 #f8fafc, 200 #e2e8f0, 600 #475569,
//           900 #0f172a) with GitHub Primer-style flat white surfaces and
//           Tailwind blue-600 #2563eb as accent; semantic colors are the
//           600/700 steps of Tailwind red, amber and green for AA contrast.
//
// Switching theme animates one blend factor (`blend`, 0 = dark, 1 = light), so
// every token, and every control bound to one, cross-fades together.
QtObject {
    id: root

    readonly property bool dark: AppSettings.theme === "dark"
        || (AppSettings.theme === "system" && Application.styleHints.colorScheme === Qt.Dark)

    // ---- Motion -----------------------------------------------------------
    readonly property bool animationsEnabled:
        Application.styleHints.accessibility.motionPreference !== Qt.ReducedMotion
    readonly property int durationFast: animationsEnabled ? 120 : 0
    readonly property int durationNormal: animationsEnabled ? 200 : 0
    readonly property int durationSlow: animationsEnabled ? 250 : 0
    readonly property int themeTransitionMs: animationsEnabled ? 100 : 0
    readonly property int easing: Easing.OutCubic

    // ---- Palettes ---------------------------------------------------------
    readonly property var darkPalette: ({
        windowBg: "#090c29",
        surface: "#0b1a42",
        surfaceRaised: "#14295f",
        border: "#2b3f85",
        divider: "#1b2c68",
        text: "#ffffff",
        textMuted: "#7bb2ce",
        accent: "#5acce6",
        accentHover: "#8adcf0",
        accentText: "#041c4a",
        danger: "#ff8a8a",
        dangerText: "#2a0a0a",
        warning: "#fbbf24",
        success: "#4ade80",
        info: "#5aa6ff",
        viewportBg: "#000000",
        overlayText: "#e2e8f0",
        tooltipBg: "#e2e8f0",
        tooltipText: "#090c29",
        scrim: "#99000000",
        hoverFill: "#14ffffff",
        pressedFill: "#24ffffff",
        checkedFill: "#335acce6"
    })
    readonly property var lightPalette: ({
        windowBg: "#f8fafc",
        surface: "#ffffff",
        surfaceRaised: "#ffffff",
        border: "#e2e8f0",
        divider: "#e2e8f0",
        text: "#0f172a",
        textMuted: "#475569",
        accent: "#2563eb",
        accentHover: "#1d4ed8",
        accentText: "#ffffff",
        danger: "#dc2626",
        dangerText: "#ffffff",
        warning: "#b45309",
        success: "#15803d",
        info: "#0369a1",
        viewportBg: "#000000",
        overlayText: "#e2e8f0",
        tooltipBg: "#0f172a",
        tooltipText: "#f8fafc",
        scrim: "#660f172a",
        hoverFill: "#0f0f172a",
        pressedFill: "#1f0f172a",
        checkedFill: "#1f2563eb"
    })

    // 0 = dark palette, 1 = light palette. Not animated until startup is over,
    // so the theme loaded from settings never visibly fades in behind the splash.
    property real blend: dark ? 0 : 1
    property bool blendAnimated: false
    Behavior on blend {
        enabled: root.blendAnimated
        NumberAnimation {
            duration: root.themeTransitionMs
            easing.type: Easing.InOutQuad
        }
    }
    Component.onCompleted: Qt.callLater(() => blendAnimated = true)

    function mix(key) {
        const a = Qt.color(darkPalette[key])
        const b = Qt.color(lightPalette[key])
        return Qt.rgba(a.r + (b.r - a.r) * blend, a.g + (b.g - a.g) * blend,
                       a.b + (b.b - a.b) * blend, a.a + (b.a - a.a) * blend)
    }

    // ---- Colour tokens ----------------------------------------------------
    readonly property color windowBg: mix("windowBg")
    readonly property color surface: mix("surface")
    readonly property color surfaceRaised: mix("surfaceRaised")
    readonly property color border: mix("border")
    readonly property color divider: mix("divider")
    readonly property color text: mix("text")
    readonly property color textMuted: mix("textMuted")
    readonly property color accent: mix("accent")
    readonly property color accentHover: mix("accentHover")
    readonly property color accentText: mix("accentText")
    readonly property color danger: mix("danger")
    readonly property color dangerText: mix("dangerText")
    readonly property color warning: mix("warning")
    readonly property color success: mix("success")
    readonly property color info: mix("info")
    readonly property color viewportBg: mix("viewportBg")
    readonly property color overlayText: mix("overlayText")
    readonly property color tooltipBg: mix("tooltipBg")
    readonly property color tooltipText: mix("tooltipText")
    readonly property color scrim: mix("scrim")
    readonly property color hoverFill: mix("hoverFill")
    readonly property color pressedFill: mix("pressedFill")
    readonly property color checkedFill: mix("checkedFill")

    // ---- Stock Material controls ------------------------------------------
    readonly property int materialTheme: dark ? Material.Dark : Material.Light
}
