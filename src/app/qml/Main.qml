import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import DicomViewer

ApplicationWindow {
    id: window

    // Startup steps, run in order while the splash is shown. The first two run
    // before the splash appears so that it already uses the saved theme and
    // language; the progress bar still counts them.
    readonly property var startupSteps: [
        { label: () => qsTr("Loading settings"), run: () => {
            AppSettings.load()
            DicomController.renderBackend = AppSettings.renderBackend === "gpu" ? DicomController.Gpu : DicomController.Cpu
        } },
        { label: () => qsTr("Loading translations"), run: () => LanguageManager.init() },
        { label: () => qsTr("Registering image codecs"), run: () => AppInfo.registerCodecs() },
        { label: () => qsTr("Creating main window"), run: () => content.active = true }
    ]
    readonly property int minSplashMs: 3000
    property int nextStep: 0
    property double splashShownAt: 0

    function runNextStep() {
        const step = startupSteps[nextStep]
        splash.status = step.label()
        step.run()
        nextStep++
        splash.progress = nextStep / startupSteps.length
    }

    visible: false
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 560
    title: qsTr("DICOM Viewer")
    Material.theme: Theme.materialTheme
    Material.accent: Theme.accent
    Material.primary: Theme.surface
    Material.background: Theme.windowBg
    Material.foreground: Theme.text

    Component.onCompleted: {
        runNextStep()
        runNextStep()
        splashShownAt = Date.now()
        splash.visible = true
        startup.start()
    }

    Timer {
        id: startup
        interval: 60
        repeat: true
        onTriggered: {
            // Steps are spread evenly over the splash time so the progress bar
            // advances at a readable pace instead of finishing instantly.
            const elapsed = Date.now() - window.splashShownAt
            const total = window.startupSteps.length
            if (window.nextStep < total) {
                if (elapsed >= window.nextStep * window.minSplashMs / total)
                    window.runNextStep()
            } else if (elapsed >= window.minSplashMs) {
                stop()
                window.show()
                splash.fadeOut()
            }
        }
    }

    Splash {
        id: splash
    }

    Loader {
        id: content
        anchors.fill: parent
        active: false

        sourceComponent: MainView {}
    }
}
