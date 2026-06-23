#version 430 core

layout (location = 0) in vec2 quadPosition;

#define POS_IDX 0
#define ROT_IDX 3
#define SCALE_IDX 7
#define OPACITY_IDX 10
#define SH_IDX 11
#define SH_DIM 3

layout (std430, binding=1) buffer gaussians_data {
    float gData[];
};
layout (std430, binding=2) buffer gaussians_order {
    int sortedGaussianIdx[];
};

vec3 get_vec3(int offset) {
    return vec3(gData[offset], gData[offset + 1], gData[offset + 2]);
}

vec4 get_vec4(int offset) {
    return vec4(gData[offset], gData[offset + 1], gData[offset + 2], gData[offset + 3]);
}

uniform float time;
uniform vec3 hfov_focal;
uniform mat4 view;
uniform mat4 projection;

out vec3 outColor;
out float opacity;
out vec3 conic;
out vec2 coordxy;

mat3 computeCov3D(vec4 rots, vec3 scales) {
    float r = rots.x;
    float x = rots.y;
    float y = rots.z;
    float z = rots.w;

    mat3 rotMatrix = mat3(
        1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + r * z),       2.0 * (x * z - r * y),
        2.0 * (x * y - r * z),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + r * x),
        2.0 * (x * z + r * y),       2.0 * (y * z - r * x),       1.0 - 2.0 * (x * x + y * y)
    );

    mat3 scaleMatrix = mat3(
        scales.x, 0.0, 0.0,
        0.0, scales.y, 0.0,
        0.0, 0.0, scales.z
    );

    mat3 mMatrix = rotMatrix * scaleMatrix;
    return mMatrix * transpose(mMatrix);
}

mat3 computeCov2D(mat3 cov3d, vec4 cam) {
    mat3 viewRot = mat3(view);
    mat3 covCam = viewRot * cov3d * transpose(viewRot);

    float invZ = 1.0 / cam.z;
    float invZ2 = invZ * invZ;
    float fx = hfov_focal.z;
    float fy = hfov_focal.z;

    float j00 = fx * invZ;
    float j02 = -fx * cam.x * invZ2;
    float j11 = fy * invZ;
    float j12 = -fy * cam.y * invZ2;

    float cov00 = j00 * j00 * covCam[0][0] +
                  2.0 * j00 * j02 * covCam[0][2] +
                  j02 * j02 * covCam[2][2];
    float cov01 = j00 * j11 * covCam[0][1] +
                  j00 * j12 * covCam[0][2] +
                  j02 * j11 * covCam[2][1] +
                  j02 * j12 * covCam[2][2];
    float cov11 = j11 * j11 * covCam[1][1] +
                  2.0 * j11 * j12 * covCam[1][2] +
                  j12 * j12 * covCam[2][2];

    return mat3(
        cov00, cov01, 0.0,
        cov01, cov11, 0.0,
        0.0,   0.0,   0.0
    );
}

void main()
{
    int quadId = sortedGaussianIdx[gl_InstanceID];
    int total_dim = 3 + 4 + 3 + 1 + SH_DIM;
    int start = quadId * total_dim;

    vec3 center = get_vec3(start + POS_IDX);
    vec4 rotations = get_vec4(start + ROT_IDX);
    vec3 scale = get_vec3(start + SCALE_IDX);
    outColor = get_vec3(start + SH_IDX);
    opacity = gData[start + OPACITY_IDX];

    mat3 cov3d = computeCov3D(rotations, scale);

    vec4 cam = view * vec4(center, 1.0);
    vec4 posClip = projection * cam;
    vec3 ndc = posClip.xyz / posClip.w;
    mat3 cov2d = computeCov2D(cov3d, cam);

    cov2d[0][0] += 0.3;
    cov2d[1][1] += 0.3;

    float trace = cov2d[0][0] + cov2d[1][1];
    float delta = sqrt(max(0.0, 0.25 * (cov2d[0][0] - cov2d[1][1]) * (cov2d[0][0] - cov2d[1][1]) +
                                  cov2d[0][1] * cov2d[0][1]));
    float det = cov2d[0][0] * cov2d[1][1] - cov2d[0][1] * cov2d[1][0];
    det = max(det, 1e-6);

    float lambda1 = max(trace * 0.5 + delta, 0.01);
    float lambda2 = max(trace * 0.5 - delta, 0.01);
    vec2 axis1 = abs(cov2d[0][1]) > 1e-6
        ? normalize(vec2(cov2d[0][1], lambda1 - cov2d[0][0]))
        : vec2(1.0, 0.0);
    vec2 axis2 = vec2(-axis1.y, axis1.x);

    float r1 = 3.0 * sqrt(lambda1);
    float r2 = 3.0 * sqrt(lambda2);
    vec2 offset_scr = quadPosition.x * r1 * axis1 +
                      quadPosition.y * r2 * axis2;

    coordxy = offset_scr;

    vec2 wh = 2.0 * hfov_focal.xy * hfov_focal.z;
    vec2 ndc_xy = ndc.xy + offset_scr / wh * 2.0;
    gl_Position = vec4(ndc_xy, ndc.z, 1.0);

    float det_inv = 1.0 / det;
    conic = vec3(cov2d[1][1] * det_inv, -cov2d[0][1] * det_inv, cov2d[0][0] * det_inv);
}
