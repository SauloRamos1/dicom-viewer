// SPDX-License-Identifier: GPL-3.0-only
// Ray-march volume material, vertex part (Qt Quick 3D CustomMaterial snippet).
// The model is the unit cube (#Cube, 100 units, scaled to the voxel aspect); the
// fragment shader marches from the camera through each fragment, so all this
// passes on is the ray direction in model space.

VARYING vec3 rayDirection; // camera to vertex, model space

void MAIN()
{
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
    rayDirection = VERTEX - (inverse(MODEL_MATRIX) * vec4(CAMERA_POSITION, 1.0)).xyz;
}
