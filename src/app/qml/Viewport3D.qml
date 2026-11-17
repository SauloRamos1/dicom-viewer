import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.Helpers
import DicomViewer

// 3D viewport: the heightmap of a single image, or the ray-marched volume of a
// series or multi-frame file, orbited by mouse. Left drag = orbit, wheel = zoom,
// Ctrl + left drag = pan (OrbitCameraController), double click = reset the
// camera, right drag = window width (horizontal) and level (vertical) like the
// 2D view. The scene is empty until an image is open.
Rectangle {
    id: root

    // Wireframe of the heightmap (the volume has no surface to outline).
    property bool wireframe: false
    readonly property bool volumeMode: DicomController.hasVolume
    // Camera distance that frames the footprint (heightmap) or the cube (volume).
    readonly property real homeDistance: 190

    function resetCamera() {
        orbit.position = Qt.vector3d(0, 0, 0)
        orbit.eulerRotation = Qt.vector3d(-35, 20, 0)
        camera.z = homeDistance
    }

    color: Theme.viewportBg
    clip: true
    Component.onCompleted: resetCamera()

    View3D {
        id: view

        anchors.fill: parent
        visible: DicomController.hasImage

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: Theme.viewportBg
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            debugSettings: DebugSettings {
                wireframeEnabled: root.wireframe && !root.volumeMode
            }
        }

        // The camera orbits the origin node, so rotating it keeps the scene centred.
        Node {
            id: orbit

            PerspectiveCamera {
                id: camera

                fieldOfView: 40
                clipNear: 1
                clipFar: 10000

                // A light that follows the view from above-left, so the relief
                // shows from every angle; the ambient term keeps shadows readable.
                DirectionalLight {
                    eulerRotation: Qt.vector3d(-35, 25, 0)
                    brightness: 0.8
                    ambientColor: "#404040"
                }
            }
        }

        HeightmapModel {
            visible: !root.volumeMode && DicomController.hasImage
        }
        VolumeModel {
            visible: root.volumeMode
        }
    }

    OrbitCameraController {
        anchors.fill: parent
        origin: orbit
        camera: camera
        enabled: DicomController.hasImage
    }

    TapHandler {
        enabled: DicomController.hasImage
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: root.resetCamera()
    }

    MouseArea {
        property point last

        anchors.fill: parent
        enabled: DicomController.windowable
        acceptedButtons: Qt.RightButton
        onPressed: event => last = Qt.point(event.x, event.y)
        onPositionChanged: event => {
            // About 200 pixels sweep the whole current width.
            const step = Math.max(1, DicomController.windowWidth / 200)
            DicomController.windowWidth += (event.x - last.x) * step
            DicomController.windowCenter += (event.y - last.y) * step
            last = Qt.point(event.x, event.y)
        }
    }

    // ---- Overlays ---------------------------------------------------------
    component Overlay: Label {
        color: Theme.overlayText
        style: Text.Outline
        styleColor: Theme.viewportBg
        font.pixelSize: 12
        opacity: 0.8
        visible: DicomController.hasImage
    }

    Overlay {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        text: root.volumeMode
            ? qsTr("Volume %1 x %2 x %3")
                .arg(DicomController.volumeSize.x).arg(DicomController.volumeSize.y).arg(DicomController.volumeSize.z)
            : qsTr("Heightmap, depth %1%").arg(Math.round(AppSettings.depthScale * 100))
    }
    Overlay {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        visible: DicomController.hasImage && DicomController.windowable
        text: qsTr("W: %1  L: %2")
            .arg(Math.round(DicomController.windowWidth))
            .arg(Math.round(DicomController.windowCenter))
    }
    Overlay {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        visible: DicomController.hasImage && !root.volumeMode && DicomController.frameCount > 1
        text: qsTr("Frame %1/%2").arg(DicomController.frame + 1).arg(DicomController.frameCount)
    }
}
