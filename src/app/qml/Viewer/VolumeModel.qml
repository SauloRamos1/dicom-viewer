// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick3D
import DicomViewer

// A series or multi-frame file as a ray-marched volume: the unit cube scaled to
// the volume's physical size (voxel spacing, so non-cubic voxels keep their
// aspect) with volume.frag marching through the R16 3D texture. The window,
// colormap and invert are the 2D view's, so they apply here too.
Model {
    id: root

    // Physical size of the volume, longest side = 1 (the cube is 100 units).
    readonly property vector3d extent: {
        const size = DicomController.volumeSize
        const spacing = DicomController.volumeSpacing
        const physical = Qt.vector3d(size.x * spacing.x, size.y * spacing.y, size.z * spacing.z)
        const longest = Math.max(physical.x, physical.y, physical.z)
        return longest > 0 ? Qt.vector3d(physical.x / longest, physical.y / longest, physical.z / longest)
                           : Qt.vector3d(1, 1, 1)
    }

    source: "#Cube"
    scale: extent
    materials: CustomMaterial {
        shadingMode: CustomMaterial.Unshaded
        // The back faces are the ones drawn, so the ray also works from inside the cube.
        cullMode: Material.FrontFaceCulling
        // The shader returns premultiplied colour.
        sourceBlend: CustomMaterial.One
        destinationBlend: CustomMaterial.OneMinusSrcAlpha
        vertexShader: "volume.vert"
        fragmentShader: "volume.frag"

        property TextureInput volume: TextureInput {
            texture: Texture {
                textureData: VolumeTexture {
                    controller: DicomController
                }
                minFilter: Texture.Linear
                magFilter: Texture.Linear
                mipFilter: Texture.None
                tilingModeHorizontal: Texture.ClampToEdge
                tilingModeVertical: Texture.ClampToEdge
                tilingModeDepth: Texture.ClampToEdge
            }
        }
        property TextureInput colormap: TextureInput {
            texture: Texture {
                textureData: ColormapTexture {
                    colormap: DicomController.colormap
                }
                minFilter: Texture.Linear
                magFilter: Texture.Linear
                mipFilter: Texture.None
                tilingModeHorizontal: Texture.ClampToEdge
                tilingModeVertical: Texture.ClampToEdge
            }
        }
        // Shader inputs, matched by name to volume.frag.
        property real windowCenter: DicomController.windowCenter
        property real windowWidth: DicomController.windowWidth
        property real valueMin: DicomController.volumeValueMin
        property real valueRange: DicomController.volumeValueMax - DicomController.volumeValueMin
        property real threshold: AppSettings.threshold
        property real densityScale: AppSettings.opacity
        property real stepSize: 1 / AppSettings.sampleSteps
        // MONOCHROME1 shows dark where the stored value is high, like Invert.
        property real invertColour: DicomController.invert !== DicomController.monochrome1 ? 1 : 0
    }
}
