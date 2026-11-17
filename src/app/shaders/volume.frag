// SPDX-License-Identifier: GPL-3.0-only
// Ray-march volume material, fragment part (Qt Quick 3D CustomMaterial snippet;
// Vulkan-style GLSL completed and cross-compiled by Qt Quick 3D at run time).
//
// The volume is one R16 3D texture (src/core/VolumeGpu.h): sample s in 0..1 is
//   value = valueMin + s * valueRange                  modality value
//   t     = clamp((value - (c - 0.5)) / (w - 1) + 0.5, 0, 1)   the 2D window
// Front to back along the ray through the cube [-50, 50]^3: a sample counts when
// t > threshold, with opacity t * densityScale per reference step (corrected for the
// real step so the look does not change with the sample count), coloured by the
// colormap at t (or 1 - t when inverted). The ray stops once it is nearly opaque.
//
// Only what qsb handles on every backend: 3D and 2D float textures, no integer
// textures, a bounded loop with break.
//
// Uniforms (CustomMaterial properties): volume (sampler3D), colormap (256x1
// sampler2D), windowCenter, windowWidth, valueMin, valueRange, threshold,
// densityScale, stepSize (texture units, 1 = the cube side), invertColour (0 or 1).

VARYING vec3 rayDirection; // camera to fragment, model space

const float kRefStep = 1.0 / 128.0;
const int kMaxSteps = 2048;

// Slab test of the ray against the cube; the entry is clamped to the camera.
bool intersectCube(vec3 origin, vec3 direction, out float tNear, out float tFar)
{
    vec3 reciprocal = 1.0 / direction;
    vec3 t0 = (vec3(-50.0) - origin) * reciprocal;
    vec3 t1 = (vec3(50.0) - origin) * reciprocal;
    vec3 lo = min(t0, t1);
    vec3 hi = max(t0, t1);
    tNear = max(0.0, max(lo.x, max(lo.y, lo.z)));
    tFar = min(hi.x, min(hi.y, hi.z));
    return tFar > tNear;
}

float windowed(float sampled)
{
    float value = valueMin + sampled * valueRange;
    float span = max(windowWidth, 1.0) - 1.0;
    float mid = windowCenter - 0.5;
    float ramp = span > 0.0 ? (value - mid) / span + 0.5 : (value > mid ? 1.0 : 0.0);
    return clamp(ramp, 0.0, 1.0);
}

void MAIN()
{
    FRAGCOLOR = vec4(0.0);

    vec3 origin = (inverse(MODEL_MATRIX) * vec4(CAMERA_POSITION, 1.0)).xyz;
    float tNear;
    float tFar;
    if (!intersectCube(origin, rayDirection, tNear, tFar))
        return;

    // Texture space: x = column, y = row (row 0 on top, so flipped), z = slice.
    vec3 start = (origin + rayDirection * tNear + vec3(50.0)) / 100.0;
    vec3 stop = (origin + rayDirection * tFar + vec3(50.0)) / 100.0;
    start.y = 1.0 - start.y;
    stop.y = 1.0 - stop.y;

    vec3 ray = stop - start;
    float rayLength = length(ray);
    if (rayLength <= 0.0)
        return;
    vec3 stepVector = ray / rayLength * stepSize;
    int count = int(min(ceil(rayLength / stepSize), float(kMaxSteps)));

    vec3 position = start + stepVector * 0.5;
    vec3 colour = vec3(0.0);
    float alpha = 0.0;
    for (int i = 0; i < count; ++i) {
        float t = windowed(textureLod(volume, position, 0.0).r);
        if (t > threshold) {
            float density = clamp(t * densityScale, 0.0, 0.999);
            float a = 1.0 - pow(1.0 - density, stepSize / kRefStep);
            float index = invertColour > 0.5 ? 1.0 - t : t;
            vec3 rgb = textureLod(colormap, vec2((index * 255.0 + 0.5) / 256.0, 0.5), 0.0).rgb;
            colour += (1.0 - alpha) * a * rgb;
            alpha += (1.0 - alpha) * a;
            if (alpha > 0.97)
                break;
        }
        position += stepVector;
    }
    FRAGCOLOR = vec4(colour, alpha); // premultiplied: blended with One / OneMinusSrcAlpha
}
