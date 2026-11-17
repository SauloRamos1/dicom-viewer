// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import DicomViewer

// The frame drawn by the GPU render path: windowing.frag turns the packed raw
// texture (image://dicomraw, uploaded once per frame) into the windowed image,
// so window, level, invert and colormap changes only move shader uniforms. It
// fills its item (View2D sizes that item to the image's displayed aspect, pixel
// aspect included) and stands in for the CPU path's Image inside View2D's
// transform items.
Item {
    id: root

    // Bilinear (true) or nearest neighbour (false) magnification, like Image.smooth.
    property bool smoothing: true

    // The packed frame. Synchronous, so a new frame, the shader uniforms and the
    // visibility switch always land in the same scene graph update. Nearest
    // filtering is required: neighbouring texels hold unrelated high/low bytes.
    Image {
        id: raw

        visible: false
        source: DicomController.hasImage && DicomController.gpuCapable
            ? "image://dicomraw/" + DicomController.gpuImageRevision : ""
        cache: false
        smooth: false
        mipmap: false
    }
    // The colormap as a 256x1 table.
    Image {
        id: lut

        visible: false
        source: "image://dicomlut/" + DicomController.colormap
        smooth: false
        mipmap: false
    }

    ShaderEffect {
        id: effect

        anchors.fill: parent
        blending: false

        // Shader inputs, matched by name to windowing.frag.
        property variant rawTex: raw
        property variant lutTex: lut
        property size texSize: Qt.size(DicomController.columns, DicomController.rows)
        property real windowCenter: DicomController.windowCenter
        property real windowWidth: DicomController.windowWidth
        property real storedMin: DicomController.storedMin
        property real slope: DicomController.rescaleSlope
        property real intercept: DicomController.rescaleIntercept
        property real monochrome1: DicomController.monochrome1 ? 1 : 0
        property real invertFlag: DicomController.invert ? 1 : 0
        property real smoothFlag: root.smoothing ? 1 : 0

        fragmentShader: "windowing.frag.qsb"
    }

    // A shader that does not load or compile means the GPU path cannot run here.
    Connections {
        target: effect

        function onStatusChanged() {
            if (effect.status === ShaderEffect.Error)
                DicomController.reportGpuUnavailable(effect.log)
        }
    }
}
