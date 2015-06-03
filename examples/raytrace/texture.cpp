//
//   Copyright 2014 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//
#include <cmath>
#include <cstdio>
#include <limits>
#include <iostream>
#include "texture.h"
#include "bezier/math.h"

#ifndef M_PI
#define M_PI 3.141592
#endif

int inline fasterfloor( const float x ) {
    if (x >= 0) {
        return (int)x;
    }

    int y = (int)x;
    if (std::abs(x - y) <= std::numeric_limits<float>::epsilon()) {
        // Do nothing.
    } else {
        y = y - 1;
    }

    return y;
}

inline void FilterByte(float* rgba,
                       const unsigned char* image,
                       int i00, int i10, int i01, int i11,
                       float w[4], // weight
                       int stride)
{
    unsigned char texel[4][4];

    const float inv = 1.0f / 255.0f;
    if (stride == 4) {

        for (int i = 0; i < 4; i++) {
            texel[0][i] = image[i00+i];
            texel[1][i] = image[i10+i];
            texel[2][i] = image[i01+i];
            texel[3][i] = image[i11+i];
        }

        for (int i = 0; i < 4; i++) {
            rgba[i] = w[0] * texel[0][i] +
                w[1] * texel[1][i] +
                w[2] * texel[2][i] +
                w[3] * texel[3][i];
            // normalize.
            rgba[i] *= inv;
        }

    } else {

        for (int i = 0; i < stride; i++) {
            texel[0][i] = image[i00+i];
            texel[1][i] = image[i10+i];
            texel[2][i] = image[i01+i];
            texel[3][i] = image[i11+i];
        }

        for (int i = 0; i < stride; i++) {
            rgba[i] = w[0] * texel[0][i] +
                w[1] * texel[1][i] +
                w[2] * texel[2][i] +
                w[3] * texel[3][i];
            // normalize.
            rgba[i] *= inv;
        }
    }

    if (stride < 4) {
        rgba[3] = 0.0;
    }

}

inline void FilterFloat(float* rgba,
                        const float* image,
                        int i00, int i10, int i01, int i11,
                        float w[4], // weight
                        int stride)
{
    float texel[4][4];

    if (stride == 4) {

        for (int i = 0; i < 4; i++) {
            texel[0][i] = image[i00+i];
            texel[1][i] = image[i10+i];
            texel[2][i] = image[i01+i];
            texel[3][i] = image[i11+i];
        }

        for (int i = 0; i < 4; i++) {
            rgba[i] = w[0] * texel[0][i] +
                w[1] * texel[1][i] +
                w[2] * texel[2][i] +
                w[3] * texel[3][i];
        }

    } else {

        for (int i = 0; i < stride; i++) {
            texel[0][i] = image[i00+i];
            texel[1][i] = image[i10+i];
            texel[2][i] = image[i01+i];
            texel[3][i] = image[i11+i];
        }

        for (int i = 0; i < stride; i++) {
            rgba[i] = w[0] * texel[0][i] +
                w[1] * texel[1][i] +
                w[2] * texel[2][i] +
                w[3] * texel[3][i];
        }
    }

    if (stride < 4) {
        rgba[3] = 0.0;
    }

}

void
Texture::fetch(float *rgba,
               float u, float v) const
{
    if (!IsValid()) {
        if (rgba) {
            rgba[0] = 0.0f;
            rgba[1] = 0.0f;
            rgba[2] = 0.0f;
            rgba[3] = 0.0f;
        }
        return;
    }

    float sx = fasterfloor(u);
    float sy = fasterfloor(v);

    float uu = u - sx;
    float vv = v - sy;

    // clamp
    uu = std::max(uu, 0.0f); uu = std::min(uu, 1.0f);
    vv = std::max(vv, 0.0f); vv = std::min(vv, 1.0f);

    float px = (m_width  - 1) * uu;
    float py = (m_height - 1) * vv;

    int x0 = (int)px;
    int y0 = (int)py;
    int x1 = ((x0 + 1) >= m_width ) ? (m_width  - 1) : (x0 + 1);
    int y1 = ((y0 + 1) >= m_height) ? (m_height - 1) : (y0 + 1);

    float dx = px - (float)x0;
    float dy = py - (float)y0;

    float w[4];

    w[0] = (1.0f - dx) * (1.0 - dy);
    w[1] = (1.0f - dx) * (      dy);
    w[2] = (       dx) * (1.0 - dy);
    w[3] = (       dx) * (      dy);

    int stride = m_components;

    int i00 = stride * (y0 * m_width + x0);
    int i01 = stride * (y0 * m_width + x1);
    int i10 = stride * (y1 * m_width + x0);
    int i11 = stride * (y1 * m_width + x1);

    if (m_format == FORMAT_BYTE) {
        FilterByte(rgba, m_image, i00, i10, i01, i11, w, stride);
    } else if (m_format == FORMAT_FLOAT) {
        FilterFloat(rgba, reinterpret_cast<const float*>(m_image),
                    i00, i10, i01, i11, w, stride);
    } else { // unknown
        printf("Unknwon format %d\n", m_format);
    }
}

void
Texture::fetchD(float *rgba0,
                float *rgba1,
                float *rgba2,
                float u, float v) const
{
    // @todo { optimize! }

    // fetch (i, j)
    fetch(rgba0, u, v);

    // fetch (i+1, j)
    float u1 = u + m_invWidth;
    fetch(rgba1, u1, v);

    // fetch (i, j+1)
    float v1 = v + m_invHeight;
    fetch(rgba2, u, v1);
}

void
LongLatMapSampler::Sample(float* rgba,
                          float  dir[3],
                          const  Texture* texture)
{
    double theta, phi;

    OsdBezier::vec3f v;
    v[0] = dir[0];
    v[1] = dir[1];
    v[2] = dir[2];
    v.normalize();

    // atan2(y, x) = 
    //
    //           y                                  y
    //       pi/2|\                             pi/2|\
    //           |                                  |
    //   pi      |       0                 pi       |        0
    //  ---------o---------> x       =>>   ---------o--------> x
    //  -pi      |      -0                 pi       |      2 pi
    //           |                                  |
    //           |                                  |
    //      -pi/2|                             3/2pi|
    //           

    phi = atan2(v[1], v[0]);
    if (phi < 0.0) {
        phi += 2.0*M_PI;            // -> now phi in [0, 2PI]
    }
    if (phi < 0.0) phi = 0.0;   // for safety.
    if (phi > 2.0 * M_PI) phi = 2.0 * M_PI;   // for safety.

    // HACK
    phi += 1.5 * M_PI;
    if (phi > 2.0 * M_PI) phi -= 2.0 * M_PI; // wrap around.

    double z = v[2];
    if (z < -1.0) z = -1.0;
    if (z > 1.0) z = 1.0;
    theta = acos(z);

    // Flip Y
    //theta = M_PI - theta;
    texture->fetch(rgba, phi / (2.0 * M_PI), theta / M_PI);
}
