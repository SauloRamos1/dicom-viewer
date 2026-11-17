#version 440

// GPU render path of the 2D view: turns the packed raw frame into the same
// image the CPU path (DCMTK) produces. The texture holds, per pixel,
//   R = high byte, G = low byte of u = stored - storedMin
// (see src/core/GpuTexture.h, whose comment is the reference of this math and
// tests/support/ShaderReference.h its C++ mirror). The steps are
//   value = (u + storedMin) * slope + intercept            modality value
//   ramp  = clamp((value - (c - 0.5)) / (w - 1) + 0.5, 0, 1)   PS3.3 C.11.2.1.2.1
//   gray  = floor(255 * ramp)  (255 * (1 - ramp) for MONOCHROME1), 255 - gray if inverted
//   rgb   = colormap[gray]
// DCMTK truncates the scaled value, hence floor and not round.
//
// Only portable GLSL (no integer textures, texelFetch or textureSize): the same
// source goes through qsb to SPIR-V, MSL, HLSL and GLSL ES 100. The texture is
// sampled with nearest filtering, so smoothing is done here: window and colormap
// are applied per texel and the four results are blended, which is what the
// hardware does for the CPU path's already windowed 8-bit image.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 texSize;       // columns, rows of the raw texture
    float windowCenter; // modality units
    float windowWidth;
    float storedMin;
    float slope;
    float intercept;
    float monochrome1;  // 1.0 for MONOCHROME1 images
    float invertFlag;   // 1.0 when Invert is on
    float smoothFlag;   // 1.0 for bilinear interpolation, 0.0 for nearest
};

layout(binding = 1) uniform sampler2D rawTex; // packed frame, nearest filtering
layout(binding = 2) uniform sampler2D lutTex; // 256x1 colormap, indexed by gray level

// Colour of the texel with integer coordinates `texel` (clamped to the image).
vec3 shade(vec2 texel)
{
    vec2 clamped = clamp(texel, vec2(0.0), texSize - vec2(1.0));
    vec4 packedValue = texture(rawTex, (clamped + vec2(0.5)) / texSize);
    float u = floor(packedValue.r * 255.0 + 0.5) * 256.0 + floor(packedValue.g * 255.0 + 0.5);
    float value = (u + storedMin) * slope + intercept;

    float span = max(windowWidth, 1.0) - 1.0;
    float mid = windowCenter - 0.5;
    float ramp = span > 0.0 ? (value - mid) / span + 0.5 : (value > mid ? 1.0 : 0.0);
    ramp = clamp(ramp, 0.0, 1.0);
    if (monochrome1 > 0.5)
        ramp = 1.0 - ramp;
    float gray = floor(ramp * 255.0);
    if (invertFlag > 0.5)
        gray = 255.0 - gray;
    return texture(lutTex, vec2((gray + 0.5) / 256.0, 0.5)).rgb;
}

void main()
{
    vec2 position = qt_TexCoord0 * texSize;
    vec3 rgb;
    if (smoothFlag > 0.5) {
        vec2 corner = position - vec2(0.5);
        vec2 base = floor(corner);
        vec2 f = corner - base;
        vec3 top = mix(shade(base), shade(base + vec2(1.0, 0.0)), f.x);
        vec3 bottom = mix(shade(base + vec2(0.0, 1.0)), shade(base + vec2(1.0, 1.0)), f.x);
        rgb = mix(top, bottom, f.y);
    } else {
        rgb = shade(floor(position));
    }
    fragColor = vec4(rgb, 1.0) * qt_Opacity;
}
