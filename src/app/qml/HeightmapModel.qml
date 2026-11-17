import QtQuick
import QtQuick3D
import DicomViewer

// A single image as a relief: the heightmap mesh of the displayed frame (height
// = windowed intensity, vertex colour = what the 2D view shows). The mesh is at
// scale 1 (heights span half the footprint's longer side either way), so the
// depth scale is only this model's y scale and changes without a rebuild.
Model {
    id: root

    geometry: HeightmapGeometry {
        controller: DicomController
    }
    scale: Qt.vector3d(1, AppSettings.depthScale, 1)
    materials: PrincipledMaterial {
        baseColor: "white"
        vertexColorsEnabled: true
        roughness: 0.85
        metalness: 0
        cullMode: Material.NoCulling
    }
}
