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
#include <cstring>
#include <cstdio>
#include <cfloat>
#include "convert_bezier.h"

#include "bezier/math.h"
#include "bezier/bezier.h"
#include "common.h"

static float ef[27] = {
    0.812816f, 0.500000f, 0.363644f, 0.287514f,
    0.238688f, 0.204544f, 0.179229f, 0.159657f,
    0.144042f, 0.131276f, 0.120632f, 0.111614f,
    0.103872f, 0.09715f, 0.0912559f, 0.0860444f,
    0.0814022f, 0.0772401f, 0.0734867f, 0.0700842f,
    0.0669851f, 0.0641504f, 0.0615475f, 0.0591488f,
    0.0569311f, 0.0548745f, 0.0529621f
};

inline float
csf(unsigned int n, unsigned int j)
{
    if (j%2 == 0) {
        return cosf((2.0f * float(M_PI) * float(float(j-0)/2.0f))/(float(n)+3.0f));
    } else {
        return sinf((2.0f * float(M_PI) * float(float(j-1)/2.0f))/(float(n)+3.0f));
    }
}

matrix4f
computeMatrixSimplified(float sharpness)
{
    float s = pow(2.0f, sharpness);
    float s2 = s*s;
    float s3 = s2*s;

    OsdBezier::matrix4f m = OsdBezier::matrix4f(
        0, s + 1 + 3*s2 - s3, 7*s - 2 - 6*s2 + 2*s3, (1-s)*(s-1)*(s-1),
        0,       (1+s)*(1+s),        6*s - 2 - 2*s2,       (s-1)*(s-1),
        0,               1+s,               6*s - 2,               1-s,
        0,                 1,               6*s - 2,                 1);

    for (int i = 0; i < 16; ++i) {
        m.m[i] /= (s*6.0);
    }
    m[0][0] = 1.0/6.0;

    return m;
}

static void convertBezier(vec3f *BP, const vec3f *P)
{
    // Watertight evaluation.
    // 0   1   2   3
    // 4   5   6   7
    // 8   9  10  11
    // 12 13  14  15
    //
    BP[0] = (((P[0] + P[10]) + (P[2] + P[8])) + (4*(P[4] + P[6]) + 4*(P[1] + P[9])))+ 16*P[5];
    BP[1] = (4*(P[4] + P[6]) + 2*(P[8] + P[10])) + 4*(4*P[5] + 2*P[9]);
    BP[2] = (4*(P[8] + P[10]) + 2*(P[4] + P[6])) + 4*(4*P[9] + 2*P[5]);
    BP[3] = (((P[12] + P[6]) + (P[4] + P[14])) + (4*(P[8] + P[10]) + 4*(P[5] + P[13]))) + 16*P[9];
    BP[4] = (4*(P[1] + P[9]) + 2*(P[2] + P[10])) + 4*(4*P[5] + 2*P[6]);
    BP[5] = (4*(4*P[5] + 2*P[6]) + 2*(2*P[10] + 4*P[9]));
    BP[6] = (2*(4*P[5] + 2*P[6]) + 4*(2*P[10] + 4*P[9]));
    BP[7] = (4*(P[5] + P[13]) + 2*(P[6] + P[14])) + 4*(4*P[9] + 2*P[10]);
    BP[8] = (4*(P[2] + P[10]) + 2*(P[1] + P[9])) + 4*(2*P[5] + 4*P[6]);
    BP[9] = (4*(2*P[5] + 4*P[6]) + 2*(4*P[10] + 2*P[9]));
    BP[10] = (2*(2*P[5] + 4*P[6]) + 4*(4*P[10] + 2*P[9]));
    BP[11] = (4*(P[6] + P[14]) + 2*(P[5] + P[13])) + 4*(4*P[10] + 2*P[9]);
    BP[12] = (((P[3] + P[9]) + (P[1] + P[11])) + (4*(P[2] + P[10]) + 4*(P[5] + P[7]))) + 16*P[6];
    BP[13] = (4*(P[5] + P[7]) + 2*(P[9] + P[11])) + 4*(4*P[6] + 2*P[10]);
    BP[14] = (4*(P[9] + P[11]) + 2*(P[5] + P[7])) + 4*(4*P[10] + 2*P[6]);
    BP[15] =(((P[5] + P[15]) + (P[7] + P[13])) + (4*(P[6]+P[14]) + 4*(P[9]+P[11]))) + 16*P[10];
    for (int j = 0; j < 16; ++j) {
        BP[j] = BP[j]*(1.0f/36.0f);
    }
}

int convertRegular(std::vector<float> &bezierVertices,
                   std::vector<int> &cpIndices,
                   float const *vertices,
                   OpenSubdiv::Far::PatchTables const *patchTables,
                   int array)
{
    int numPatches = patchTables->GetNumPatches(array);

    // regular to bezier
    for (int i = 0; i < numPatches; i++) {
        const int *verts = &patchTables->GetPatchArrayVertices(array)[i*16];

        vec3f P[16];
        for (int j = 0; j < 16; ++j) {
            P[j] = vec3f(&vertices[verts[j]*3]);
        }
        vec3f BP[16];
        convertBezier(BP, P);

        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(BP[j][0]);
            bezierVertices.push_back(BP[j][1]);
            bezierVertices.push_back(BP[j][2]);
        }

        // save center quad indices
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 5]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 6]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 9]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 10]);
    }
    return numPatches;
}

int convertSingleCrease(std::vector<float> &bezierVertices,
                        std::vector<int> &cpIndices,
                        float const *vertices,
                        OpenSubdiv::Far::PatchTables const *patchTables,
                        int array)
{
    int numPatches = patchTables->GetNumPatches(array);

    // single-crease to bezier.
    for (int i = 0; i < numPatches; i++) {
        const int *verts = &patchTables->GetPatchArrayVertices(array)[i*16];
        //const int *verts = &patchTables->GetPatchTable()[parray.GetVertIndex() + i*16];

        // sharpness matrix
        int index = patchTables->GetSharpnessIndices(array)[i];
        //int index = patchTables->GetSharpnessIndexTable()[parray.GetPatchIndex() + i];
        float sharpness = patchTables->GetSharpnessValues()[index];
        //float Sf = floor(sharpness);
        //float Sc = ceil(sharpness);
        //float Sr = sharpness - Sf;
        //matrix4t<float> Mf = computeMatrixSimplified(Sf);
        //matrix4t<float> Mc = computeMatrixSimplified(Sc);
        //matrix4t<float> Mj = (1-Sr) * Mf + Sr * Mi;

        matrix4f Ms = computeMatrixSimplified(sharpness);
        vec3f cp0[16], cp1[16];

        vec3f P[16];
        for (int j = 0; j < 16; ++j) {
            P[j] = vec3f(&vertices[verts[j]*3]);
        }

        /*
          m0 m1 m2 m3
             m5 m6 m7
             m9 m10 m11
             m13 m14 m15
         */
        float m0 = Ms[0][0];
        float m1 = Ms[0][1];
        float m2 = Ms[0][2];
        float m3 = Ms[0][3];
        //float m4 = Ms[1][0];
        float m5 = Ms[1][1];
        float m6 = Ms[1][2];
        float m7 = Ms[1][3];
        //float m8 = Ms[2][0];
        float m9 = Ms[2][1];
        float m10 = Ms[2][2];
        float m11 = Ms[2][3];
        //float m12 = Ms[3][0];
        float m13 = Ms[3][1];
        float m14 = Ms[3][2];
        float m15 = Ms[3][3];
        vec3f P0 = P[0];
        vec3f P1 = P[4];
        vec3f P2 = P[8];
        vec3f P3 = P[12];
        vec3f P4 = P[1];
        vec3f P5 = P[5];
        vec3f P6 = P[9];
        vec3f P7 = P[13];
        vec3f P8 = P[2];
        vec3f P9 = P[6];
        vec3f P10 = P[10];
        vec3f P11 = P[14];
        vec3f P12 = P[3];
        vec3f P13 = P[7];
        vec3f P14 = P[11];
        vec3f P15 = P[15];

        // reversal invariant
        // 0   1   2   3 |
        // 4   5   6   7 |(crease)
        // 8   9  10  11 |
        // 12  13  14  15|
        cp0[0]  = m2 * (P10 + P2) + 4*m2*P6  + m3* (P11 + P3) + 4*m3*P7  + m0* (P0 + P8) + 4*m0*P4 + m1*(P1 + P9) + 4*m1*P5;
        cp0[12] = m2 * (P14 + P6) + 4*m2*P10 + m3* (P15 + P7) + 4*m3*P11 + m0* (P12 + P4) + 4*m0*P8 + m1* (P13 + P5) + 4*m1*P9;

        cp0[1]  = m6 * (P10 + P2) + 4*m6*P6 + m7* (P11 + P3) + 4*m7*P7 + m5* (P1 + P9) + 4*m5*P5;
        cp0[13] = m6 * (P14 + P6) + 4*m6*P10 + m7* (P15 + P7) + 4*m7*P11 + m5* (P13 + P5) + 4*m5*P9;

        cp0[2]  = m10 * (P10 + P2) + 4*m10*P6 + m11* (P11 + P3) + 4*m11*P7 + m9* (P1+P9) + 4*m9*P5;
        cp0[14] = m10 * (P14 + P6) + 4*m10*P10 + m11* (P15 + P7) + 4*m11*P11 + m9* (P13+P5) + 4*m9*P9;

        cp0[6]  = m10 * (2* P10 + 4* P6) + m11 * (2* P11 + 4* P7) + m9* (4* P5 + 2* P9);
        cp0[10] = m10 * (4* P10 + 2* P6) + m11 * (4* P11 + 2* P7) + m9* (2* P5 + 4* P9);

        cp0[4]  = m2  * (2* P10 + 4* P6) + m3 * (2* P11 + 4* P7) + m0* (4* P4 + 2* P8) + m1* (4* P5 + 2* P9);
        cp0[8]  = m2  * (4* P10 + 2* P6) + m3 * (4* P11 + 2* P7) + m0* (2* P4 + 4* P8) + m1* (2* P5 + 4* P9);

        cp0[5]  = m6  * (2* P10 + 4* P6) + m7* (2* P11 + 4* P7) + m5* (4* P5 + 2* P9);
        cp0[9]  = m6  * (4* P10 + 2* P6) + m7* (4* P11 + 2* P7) + m5* (2* P5 + 4* P9);

        cp0[7]  = m14 * (2* P10 + 4* P6) + m15* (2* P11 + 4* P7) +  m13* (4* P5 + 2* P9);
        cp0[11] = m14 * (4* P10 + 2* P6) + m15* (4* P11 + 2* P7) + m13* (2* P5 + 4* P9);
        cp0[3]  = m14*(P10 + P2) + 4*m14*P6  + m15*(P11 + P3) + 4*m15*P7  + m13*(P1 + P9) + 4*m13*P5;
        cp0[15] = m14*(P14 + P6) + 4*m14*P10 + m15*(P15 + P7) + 4*m15*P11 + m13*(P13 + P5) + 4*m13*P9;

        for (int j = 0; j < 16; ++j) {
            cp0[j] = cp0[j]*(1.0f/6.0f);
        }

        // Minfinite
        // 0   1   2   (3) |
        // 4   5   6   (7) |(crease)
        // 8   9  10  (11) |
        // 12  13  14  (15)|
        cp1[0]  = ((P0 + P10) + (P2 + P8)) + 4 * ((P4 + P6) + (P1 + P9) + 4 * P5);
        cp1[4]  = (2*P10 + 4*P6) + (4*P4 + 2*P8) + 4*(4*P5 + 2*P9);
        cp1[8]  = (4*P10 + 2*P6) + (2*P4 + 4*P8) + 4*(2*P5 + 4*P9);
        cp1[12] = ((P12 + P6) + (P14 + P4)) + 4 * ((P10 + P8) + (P13 + P5) + 4 *P9);

        // is this good...?
        cp1[1]  = 2*((P10 + P2) + 4*P6) + 4*((P1 + P9) + 4*P5);
        cp1[2]  = 4*((P10 + P2) + 4*P6) + 2*((P1 + P9) + 4*P5);
        cp1[13] = 2*(4*P10 + (P14 + P6)) + 4*((P13 + P5) + 4*P9);
        cp1[14] = 4*(4*P10 + (P14 + P6)) + 2*((P13 + P5) + 4*P9);

        cp1[3]  = 6 * ((P10 + P2) + 4 *P6);
        cp1[15] = 6 * (4*P10 + (P14 + P6));

        cp1[7]  = 6 * (2*P10 + 4*P6);
        cp1[11] = 6 * (4*P10 + 2*P6);

        cp1[5]  = 2 * (2*P10 + 4*P6) + 4 * (4*P5 + 2*P9);
        cp1[6]  = 4 * (2*P10 + 4*P6) + 2 * (4*P5 + 2*P9);
        cp1[9]  = 2 * (4*P10 + 2*P6) + 4 * (2*P5 + 4*P9);
        cp1[10] = 4 * (4*P10 + 2*P6) + 2 * (2*P5 + 4*P9);

        for (int j = 0; j < 16; ++j) {
            cp1[j] = cp1[j]*(1.0f/36.0f);
        }

        // split patch
        OsdBezier::BezierPatch<vec3f, float, 4> patch0(cp0);
        OsdBezier::BezierPatch<vec3f, float, 4> patch1(cp1);

        OsdBezier::BezierPatch<vec3f, float, 4> patch0s[2];
        OsdBezier::BezierPatch<vec3f, float, 4> patch1s[2];
        float t = 1.0 - pow(2.0f, -sharpness);
        patch0.SplitU(patch0s, t);
        patch1.SplitU(patch1s, t);

        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                cp0[j*4+k] = patch0s[1].Get(k, j);
                cp1[j*4+k] = patch1s[0].Get(k, j);
            }
        }

        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(cp0[j][0]);
            bezierVertices.push_back(cp0[j][1]);
            bezierVertices.push_back(cp0[j][2]);
        }
        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(cp1[j][0]);
            bezierVertices.push_back(cp1[j][1]);
            bezierVertices.push_back(cp1[j][2]);
        }

#if 0
        // save center quad indices
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 5]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 6]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 9]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 10]);
        // save center quad indices
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 5]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 6]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 9]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*16 + 10]);
#endif
        for (int j = 0; j < 8; ++j) {
            cpIndices.push_back(-1);
        }

    }
    return numPatches*2;
}

int convertBoundary(std::vector<float> &bezierVertices,
                    std::vector<int> &cpIndices,
                    float const *vertices,
                    OpenSubdiv::Far::PatchTables const *patchTables,
                    int array)
{
    int numPatches = patchTables->GetNumPatches(array);

    // bounadry to bezier
    for (int i = 0; i < numPatches; i++) {
        const int *verts = &patchTables->GetPatchArrayVertices(array)[i*12];

        vec3f BP[16];
        // -   -   -   -
        // 0   1   2   3
        // 4   5   6   7
        // 8   9  10  11
        vec3f P[16];
        for (int j = 0; j < 12; ++j) {
            P[j+4] = vec3f(&vertices[verts[j]*3]);
        }

        // mirroring boundary control points
        P[0] = 2*P[4]-P[8];
        P[1] = 2*P[5]-P[9];
        P[2] = 2*P[6]-P[10];
        P[3] = 2*P[7]-P[11];

        convertBezier(BP, P);

        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(BP[j][0]);
            bezierVertices.push_back(BP[j][1]);
            bezierVertices.push_back(BP[j][2]);
        }

        // save center quad indices
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*12 + 1]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*12 + 2]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*12 + 5]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*12 + 6]);
    }
    return numPatches;
}

int convertCorner(std::vector<float> &bezierVertices,
                  std::vector<int> &cpIndices,
                  float const *vertices,
                  OpenSubdiv::Far::PatchTables const *patchTables,
                  int array)
{
    int numPatches = patchTables->GetNumPatches(array);

    for (int i = 0; i < numPatches; i++) {
        const int *verts = &patchTables->GetPatchArrayVertices(array)[i*9];

        vec3f BP[16];
        // -   -   -   -     0  1  2  3
        // 0   1   2   -     4  5  6  7
        // 3   4   5   -     8  9 10 11
        // 6   7   8   -    12 13 14 15
        vec3f P[16];
        P[4] = vec3f(&vertices[verts[0]*3]);
        P[5] = vec3f(&vertices[verts[1]*3]);
        P[6] = vec3f(&vertices[verts[2]*3]);
        P[8] = vec3f(&vertices[verts[3]*3]);
        P[9] = vec3f(&vertices[verts[4]*3]);
        P[10] = vec3f(&vertices[verts[5]*3]);
        P[12] = vec3f(&vertices[verts[6]*3]);
        P[13] = vec3f(&vertices[verts[7]*3]);
        P[14] = vec3f(&vertices[verts[8]*3]);

        // mirroring boundary control points
        P[0] = 2*P[4]-P[8];
        P[1] = 2*P[5]-P[9];
        P[2] = 2*P[6]-P[10];
        P[3] = 2*P[6]-P[9];
        P[7] = 2*P[6]-P[5];
        P[11] = 2*P[10]-P[9];
        P[15] = 2*P[14]-P[13];

        convertBezier(BP, P);

        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(BP[j][0]);
            bezierVertices.push_back(BP[j][1]);
            bezierVertices.push_back(BP[j][2]);
        }

        // save center quad indices
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*9 + 1]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*9 + 2]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*9 + 4]);
        cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[i*9 + 5]);
    }
    return numPatches;
}


int convertGregory(std::vector<float> &bezierVertices,
                   std::vector<int> &cpIndices,
                   float const *vertices,
                   OpenSubdiv::Far::PatchTables const *patchTables,
                   int array)
{
    int const * vertexValenceBuffer = &patchTables->GetVertexValenceTable()[0];
    int valences[4];
    int length = 3;
    int stride = 3;
    int maxValence = patchTables->GetMaxValence();
    //printf("Max Valence = %d\n", maxValence);
    float *r = (float*)alloca((maxValence+2)*4*length*sizeof(float));
    float *f = (float*)alloca(maxValence*length*sizeof(float)),
        *pos = (float*)alloca(length*sizeof(float)),
        *opos = (float*)alloca(length*4*sizeof(float));
    float const * inOffset = (float const*)&vertices[0];

    float *Ep=(float*)alloca(length*4*sizeof(float)),
        *Em=(float*)alloca(length*4*sizeof(float)),
        *Fp=(float*)alloca(length*4*sizeof(float)),
        *Fm=(float*)alloca(length*4*sizeof(float));
    float *Em_ip=(float*)alloca(length*sizeof(float)),
        *Ep_im=(float*)alloca(length*sizeof(float));
    float *q=(float*)alloca(length*16*sizeof(float));

    int numPatches = patchTables->GetNumPatches(array);
    for (int patchIndex = 0; patchIndex < numPatches; ++patchIndex) {

        int const * vertexIndices = &patchTables->GetPatchArrayVertices(array)[patchIndex*4];
        //int const *quadOffsetBuffer = &patchTables->GetQuadOffsetTable()[parray.GetQuadOffsetIndex() + patchIndex*4];
        OpenSubdiv::Far::PatchTables::PatchHandle handle;
        handle.arrayIndex = array;
        handle.patchIndex = patchIndex;
        handle.vertIndex = patchIndex*4;
        OpenSubdiv::Vtr::ConstArray<unsigned int> quadOffsets = patchTables->GetPatchQuadOffsets(handle);

        bool badPatch = false;

        float *rp,
            *e0 = r + maxValence*4*length,
            *e1 = e0 + 4*length;
        memset(r, 0, (maxValence+2)*4*length*sizeof(float));

        memset(opos, 0, length*4*sizeof(float));

        for (int vid=0; vid < 4; ++vid) {

            int vertexID = vertexIndices[vid];

            const int *valenceTable = vertexValenceBuffer + vertexID * (2*maxValence+1);
            int valence = abs(*valenceTable);
            assert(valence<=maxValence);
            valences[vid] = valence;

            memcpy(pos, inOffset + vertexID*stride, length*sizeof(float));

            rp=r+vid*maxValence*length;

            int vofs = vid*length;

            for (int i=0; i<valence; ++i) {
                unsigned int im = (i+valence-1)%valence,
                    ip = (i+1)%valence;

                int idx_neighbor   = valenceTable[2*i  + 0 + 1];
                int idx_diagonal   = valenceTable[2*i  + 1 + 1];
                int idx_neighbor_p = valenceTable[2*ip + 0 + 1];
                int idx_neighbor_m = valenceTable[2*im + 0 + 1];
                int idx_diagonal_m = valenceTable[2*im + 1 + 1];

                float const * neighbor   = inOffset + idx_neighbor   * stride;
                float const * diagonal   = inOffset + idx_diagonal   * stride;
                float const * neighbor_p = inOffset + idx_neighbor_p * stride;
                float const * neighbor_m = inOffset + idx_neighbor_m * stride;
                float const * diagonal_m = inOffset + idx_diagonal_m * stride;

                float  *fp = f+i*length;

                for (int k=0; k<length; ++k) {
                    fp[k] = (pos[k]*float(valence) + (neighbor_p[k]+neighbor[k])*2.0f + diagonal[k])/(float(valence)+5.0f);

                    opos[vofs+k] += fp[k];
                    rp[i*length+k] =(neighbor_p[k]-neighbor_m[k])/3.0f + (diagonal[k]-diagonal_m[k])/6.0f;
                }

            }

            for (int k=0; k<length; ++k) {
                opos[vofs+k] /= valence;
            }

            if (valence >= 3 and valence < 30) {
                for (int i=0; i<valence; ++i) {
                    int im = (i+valence-1)%valence;
                    for (int k=0; k<length; ++k) {
                        float e = 0.5f*(f[i*length+k]+f[im*length+k]);
                        e0[vofs+k] += csf(valence-3, 2*i) * e;
                        e1[vofs+k] += csf(valence-3, 2*i+1) * e;
                    }
                }
                for (int k=0; k<length; ++k) {
                    e0[vofs+k] *= ef[valence-3];
                    e1[vofs+k] *= ef[valence-3];
                }
            } else {
                badPatch = true;
            }
        }

        // tess control

        // Control Vertices based on :
        // "Approximating Subdivision Surfaces with Gregory Patches for Hardware Tessellation"
        // Loop, Schaefer, Ni, Castafio (ACM ToG Siggraph Asia 2009)
        //
        //  P3         e3-      e2+         E2
        //     O--------O--------O--------O
        //     |        |        |        |
        //     |        |        |        |
        //     |        | f3-    | f2+    |
        //     |        O        O        |
        // e3+ O------O            O------O e2-
        //     |     f3+          f2-     |
        //     |                          |
        //     |                          |
        //     |      f0-         f1+     |
        // e0- O------O            O------O e1+
        //     |        O        O        |
        //     |        | f0+    | f1-    |
        //     |        |        |        |
        //     |        |        |        |
        //     O--------O--------O--------O
        //  P0         e0+      e1-         E1
        //

        for (int vid=0; vid<4; ++vid) {

            int ip = (vid+1)%4;
            int im = (vid+3)%4;
            int n = valences[vid];
            //int const *quadOffsets = quadOffsetBuffer;

            int start = quadOffsets[vid] & 0x00ff;
            int prev = (quadOffsets[vid] & 0xff00) / 256;

            for (int k=0, ofs=vid*length; k<length; ++k, ++ofs) {

                Ep[ofs] = opos[ofs] + e0[ofs] * csf(n-3, 2*start) + e1[ofs]*csf(n-3, 2*start +1);
                Em[ofs] = opos[ofs] + e0[ofs] * csf(n-3, 2*prev ) + e1[ofs]*csf(n-3, 2*prev + 1);

            }

            unsigned int np = valences[ip],
                nm = valences[im];

            unsigned int prev_p = (quadOffsets[ip] & 0xff00) / 256,
                start_m = quadOffsets[im] & 0x00ff;

            for (int k=0, ipofs=ip*length, imofs=im*length; k<length; ++k, ++ipofs, ++imofs) {
                Em_ip[k] = opos[ipofs] + e0[ipofs]*csf(np-3, 2*prev_p)  + e1[ipofs]*csf(np-3, 2*prev_p+1);
                Ep_im[k] = opos[imofs] + e0[imofs]*csf(nm-3, 2*start_m) + e1[imofs]*csf(nm-3, 2*start_m+1);
            }

            float s1 = 3.0f - 2.0f*csf(n-3,2)-csf(np-3,2),
                s2 = 2.0f*csf(n-3,2),
                s3 = 3.0f -2.0f*cosf(2.0f*float(M_PI)/float(n)) - cosf(2.0f*float(M_PI)/float(nm));

            rp = r + vid*maxValence*length;
            for (int k=0, ofs=vid*length; k<length; ++k, ++ofs) {
                Fp[ofs] = (csf(np-3,2)*opos[ofs] + s1*Ep[ofs] + s2*Em_ip[k] + rp[start*length+k])/3.0f;
                Fm[ofs] = (csf(nm-3,2)*opos[ofs] + s3*Em[ofs] + s2*Ep_im[k] - rp[prev*length+k])/3.0f;
            }
        }

        float * p[20];
        for (int i=0, ofs=0; i<4; ++i, ofs+=length) {
            p[i*5+0] = opos + ofs;
            p[i*5+1] =   Ep + ofs;
            p[i*5+2] =   Em + ofs;
            p[i*5+3] =   Fp + ofs;
            p[i*5+4] =   Fm + ofs;
        }

        float u = 0.5f, v=0.5f;
        float U = 1-u, V=1-v;
        float d11 = u+v; if(u+v==0.0f) d11 = 1.0f;
        float d12 = U+v; if(U+v==0.0f) d12 = 1.0f;
        float d21 = u+V; if(u+V==0.0f) d21 = 1.0f;
        float d22 = U+V; if(U+V==0.0f) d22 = 1.0f;

        for (int k=0; k<length; ++k) {
            q[ 5*length+k] = (u*p[ 3][k] + v*p[ 4][k])/d11;
            q[ 6*length+k] = (U*p[ 9][k] + v*p[ 8][k])/d12;
            q[ 9*length+k] = (u*p[19][k] + V*p[18][k])/d21;
            q[10*length+k] = (U*p[13][k] + V*p[14][k])/d22;
        }

        memcpy(q+ 0*length, p[ 0], length*sizeof(float));
        memcpy(q+ 1*length, p[ 1], length*sizeof(float));
        memcpy(q+ 2*length, p[ 7], length*sizeof(float));
        memcpy(q+ 3*length, p[ 5], length*sizeof(float));
        memcpy(q+ 4*length, p[ 2], length*sizeof(float));
        memcpy(q+ 7*length, p[ 6], length*sizeof(float));
        memcpy(q+ 8*length, p[16], length*sizeof(float));
        memcpy(q+11*length, p[12], length*sizeof(float));
        memcpy(q+12*length, p[15], length*sizeof(float));
        memcpy(q+13*length, p[17], length*sizeof(float));
        memcpy(q+14*length, p[11], length*sizeof(float));
        memcpy(q+15*length, p[10], length*sizeof(float));

        if (badPatch) {
            memset(q, 0, 16*3*sizeof(float));
        }

        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                int index = y*4 + x;
                bezierVertices.push_back(q[index*3+0]);
                bezierVertices.push_back(q[index*3+1]);
                bezierVertices.push_back(q[index*3+2]);
            }
        }

        // save center quad indices
        if (!badPatch) {
            cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[patchIndex*4 + 0]);
            cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[patchIndex*4 + 1]);
            cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[patchIndex*4 + 2]);
            cpIndices.push_back(patchTables->GetPatchArrayVertices(array)[patchIndex*4 + 3]);
        } else {
            cpIndices.push_back(-1);
            cpIndices.push_back(-1);
            cpIndices.push_back(-1);
            cpIndices.push_back(-1);
        }
    }

    return patchTables->GetNumPatches(array);
}

int convertBoundaryGregory(std::vector<float> &bezierVertices,
                           std::vector<int> &cpIndices,
                           float const *vertices,
                           OpenSubdiv::Far::PatchTables const *patchTables,
                           int array)
{
    int const * vertexValenceBuffer = &patchTables->GetVertexValenceTable()[0];
    int valences[4], zerothNeighbors[4];
    int length = 3;
    int stride = 3;
    int maxValence = patchTables->GetMaxValence();
    float const * inOffset = (float const*)&vertices[0];
    float  *r  = (float*)alloca((maxValence+2)*4*length*sizeof(float));
    float *f=(float*)alloca(maxValence*length*sizeof(float)),
        *org=(float*)alloca(length*4*sizeof(float)),
        *opos=(float*)alloca(length*4*sizeof(float));
    float *Ep=(float*)alloca(length*4*sizeof(float)),
        *Em=(float*)alloca(length*4*sizeof(float)),
        *Fp=(float*)alloca(length*4*sizeof(float)),
        *Fm=(float*)alloca(length*4*sizeof(float));
    float *Em_ip=(float*)alloca(length*sizeof(float)),
        *Ep_im=(float*)alloca(length*sizeof(float));
    float *q=(float*)alloca(length*16*sizeof(float));

    int numPatches = patchTables->GetNumPatches(array);
    for (int patchIndex = 0; patchIndex < numPatches; ++patchIndex) {

        // vertex
        int const * vertexIndices = &patchTables->GetPatchArrayVertices(array)[patchIndex*4];
        //int const * quadOffsets = &patchTables->GetQuadOffsetTable()[parray.GetQuadOffsetIndex() + patchIndex*4];
        OpenSubdiv::Far::PatchTables::PatchHandle handle;
        handle.arrayIndex = array;
        handle.patchIndex = patchIndex;
        handle.vertIndex = patchIndex*4;
        OpenSubdiv::Vtr::ConstArray<unsigned int> quadOffsets = patchTables->GetPatchQuadOffsets(handle);

        bool badPatch = false;

        float *rp,
            *e0 = r + maxValence*4*length,
            *e1 = e0 + 4*length;
        memset(r, 0, (maxValence+2)*4*length*sizeof(float));

        memset(opos, 0, length*4*sizeof(float));

        for (int vid=0; vid < 4; ++vid) {

            int vertexID = vertexIndices[vid];

            const int *valenceTable = vertexValenceBuffer + vertexID * (2*maxValence+1);
            int valence = *valenceTable,
                ivalence = abs(valence);

            assert(ivalence<=maxValence);
            valences[vid] = valence;

            int vofs = vid * length;

            float *pos=org + vofs;
            memcpy(pos, inOffset + vertexID*stride, length*sizeof(float));

            int boundaryEdgeNeighbors[2];
            unsigned int currNeighbor = 0,
                ibefore=0,
                zerothNeighbor=0;

            rp=r+vid*maxValence*length;

            for (int i=0; i<ivalence; ++i) {
                unsigned int im = (i+ivalence-1)%ivalence,
                    ip = (i+1)%ivalence;

                int idx_neighbor   = valenceTable[2*i  + 0 + 1];
                int idx_diagonal   = valenceTable[2*i  + 1 + 1];
                int idx_neighbor_p = valenceTable[2*ip + 0 + 1];
                int idx_neighbor_m = valenceTable[2*im + 0 + 1];
                int idx_diagonal_m = valenceTable[2*im + 1 + 1];

                int valenceNeighbor = vertexValenceBuffer[idx_neighbor * (2*maxValence+1)];
                if (valenceNeighbor < 0) {

                    if (currNeighbor<2) {
                        boundaryEdgeNeighbors[currNeighbor] = idx_neighbor;
                    }
                    currNeighbor++;

                    if (currNeighbor == 1)    {
                        ibefore = i;
                        zerothNeighbor = i;
                    } else {
                        if (i-ibefore == 1) {
                            int tmp = boundaryEdgeNeighbors[0];
                            boundaryEdgeNeighbors[0] = boundaryEdgeNeighbors[1];
                            boundaryEdgeNeighbors[1] = tmp;
                            zerothNeighbor = i;
                        }
                    }
                }

                float const * neighbor   = inOffset + idx_neighbor   * stride;
                float const * diagonal   = inOffset + idx_diagonal   * stride;
                float const * neighbor_p = inOffset + idx_neighbor_p * stride;
                float const * neighbor_m = inOffset + idx_neighbor_m * stride;
                float const * diagonal_m = inOffset + idx_diagonal_m * stride;

                float *fp = f+i*length;

                for (int k=0; k<length; ++k) {
                    fp[k] = (pos[k]*float(ivalence) + (neighbor_p[k]+neighbor[k])*2.0f + diagonal[k])/(float(ivalence)+5.0f);

                    opos[vofs+k] += fp[k];
                    rp[i*length+k] =(neighbor_p[k]-neighbor_m[k])/3.0f + (diagonal[k]-diagonal_m[k])/6.0f;
                }
            }

            for (int k=0; k<length; ++k) {
                opos[vofs+k] /= ivalence;
            }

            zerothNeighbors[vid] = zerothNeighbor;

            if (currNeighbor == 1) {
                boundaryEdgeNeighbors[1] = boundaryEdgeNeighbors[0];
            }

            if (ivalence >= 3) {
                for (int i=0; i<ivalence; ++i) {
                    unsigned int im = (i+ivalence-1)%ivalence;
                    for (int k=0; k<length; ++k) {
                        float e = 0.5f*(f[i*length+k]+f[im*length+k]);
                        e0[vofs+k] += csf(ivalence-3, 2*i  ) * e;
                        e1[vofs+k] += csf(ivalence-3, 2*i+1) * e;
                    }
                }
                for (int k=0; k<length; ++k) {
                    e0[vofs+k] *= ef[ivalence-3];
                    e1[vofs+k] *= ef[ivalence-3];
                }
            } else {
                badPatch = true;
            }

            if (valence<0) {
                if (ivalence>2) {
                    for (int k=0; k<length; ++k) {
                        opos[vofs+k] = (inOffset[boundaryEdgeNeighbors[0]*stride+k] +
                                        inOffset[boundaryEdgeNeighbors[1]*stride+k] + 4.0f*pos[k])/6.0f;
                    }
                } else {
                    memcpy(opos, pos, length*sizeof(float));
                }

                float k = float(float(ivalence) - 1.0f);    //k is the number of faces
                float c = cosf(float(M_PI)/k);
                float s = sinf(float(M_PI)/k);
                float gamma = -(4.0f*s)/(3.0f*k+c);
                float alpha_0k = -((1.0f+2.0f*c)*sqrtf(1.0f+c))/((3.0f*k+c)*sqrtf(1.0f-c));
                float beta_0 = s/(3.0f*k + c);

                int idx_diagonal = valenceTable[2*zerothNeighbor + 1 + 1];
                assert(idx_diagonal>0);
                float const * diagonal = inOffset + idx_diagonal * stride;

                for (int j=0; j<length; ++j) {
                    e0[vofs+j] = (inOffset[boundaryEdgeNeighbors[0]*stride+j] -
                                  inOffset[boundaryEdgeNeighbors[1]*stride+j])/6.0f;

                    e1[vofs+j] = gamma * pos[j] + beta_0 * diagonal[j] +
                        (inOffset[boundaryEdgeNeighbors[0]*stride+j] +
                         inOffset[boundaryEdgeNeighbors[1]*stride+j]) * alpha_0k;

                }

                for (int x=1; x<ivalence-1; ++x) {
                    unsigned int curri = ((x + zerothNeighbor)%ivalence);
                    float alpha = (4.0f*sinf((float(M_PI) * float(x))/k))/(3.0f*k+c);
                    float beta = (sinf((float(M_PI) * float(x))/k) + sinf((float(M_PI) * float(x+1))/k))/(3.0f*k+c);

                    int idx_neighbor = valenceTable[2*curri + 0 + 1];
                    idx_diagonal = valenceTable[2*curri + 1 + 1];
                    assert( idx_neighbor>0 and idx_diagonal>0 );

                    float const * neighbor = inOffset + idx_neighbor * stride;
                    diagonal = inOffset + idx_diagonal * stride;

                    for (int j=0; j<length; ++j) {
                        e1[vofs+j] += alpha*neighbor[j] + beta*diagonal[j];
                    }
                }

                for (int j=0; j<length; ++j) {
                    e1[vofs+j] /= 3.0f;
                }
            }
        }

        // tess control

        // Control Vertices based on :
        // "Approximating Subdivision Surfaces with Gregory Patches for Hardware Tessellation"
        // Loop, Schaefer, Ni, Castafio (ACM ToG Siggraph Asia 2009)
        //
        //  P3         e3-      e2+         E2
        //     O--------O--------O--------O
        //     |        |        |        |
        //     |        |        |        |
        //     |        | f3-    | f2+    |
        //     |        O        O        |
        // e3+ O------O            O------O e2-
        //     |     f3+          f2-     |
        //     |                          |
        //     |                          |
        //     |      f0-         f1+     |
        // e0- O------O            O------O e1+
        //     |        O        O        |
        //     |        | f0+    | f1-    |
        //     |        |        |        |
        //     |        |        |        |
        //     O--------O--------O--------O
        //  P0         e0+      e1-         E1
        //

        for (int vid=0; vid<4; ++vid) {

            unsigned int ip = (vid+1)%4,
                im = (vid+3)%4,
                n = abs(valences[vid]),
                ivalence = n;

            //const int *quadOffsets = quadOffsetBuffer;

            int vofs = vid * length;

            unsigned int   start =  quadOffsets[vid] & 0x00ff,
                prev = (quadOffsets[vid] & 0xff00) / 256,
                np = abs(valences[ip]),
                nm = abs(valences[im]),
                start_m =  quadOffsets[im] & 0x00ff,
                prev_p = (quadOffsets[ip] & 0xff00) / 256;

            if (valences[ip]<-2) {
                unsigned int j = (np + prev_p - zerothNeighbors[ip]) % np;
                for (int k=0, ipofs=ip*length; k<length; ++k, ++ipofs) {
                    Em_ip[k] = opos[ipofs] + cosf((float(M_PI)*j)/float(np-1))*e0[ipofs] + sinf((float(M_PI)*j)/float(np-1))*e1[ipofs];
                }
            } else {
                for (int k=0, ipofs=ip*length; k<length; ++k, ++ipofs) {
                    Em_ip[k] = opos[ipofs] + e0[ipofs]*csf(np-3,2*prev_p)  + e1[ipofs]*csf(np-3,2*prev_p+1);
                }
            }

            if (valences[im]<-2) {
                unsigned int j = (nm + start_m - zerothNeighbors[im]) % nm;
                for (int k=0, imofs=im*length; k<length; ++k, ++imofs) {
                    Ep_im[k] = opos[imofs] + cosf((float(M_PI)*j)/float(nm-1))*e0[imofs] + sinf((float(M_PI)*j)/float(nm-1))*e1[imofs];
                }
            } else {
                for (int k=0, imofs=im*length; k<length; ++k, ++imofs) {
                    Ep_im[k] = opos[imofs] + e0[imofs]*csf(nm-3,2*start_m) + e1[imofs]*csf(nm-3,2*start_m+1);
                }
            }

            if (valences[vid] < 0) {
                n = (n-1)*2;
            }
            if (valences[im] < 0) {
                nm = (nm-1)*2;
            }
            if (valences[ip] < 0) {
                np = (np-1)*2;
            }

            rp=r+vid*maxValence*length;

            if (valences[vid] > 2) {
                float s1 = 3.0f - 2.0f*csf(n-3,2)-csf(np-3,2),
                    s2 = 2.0f*csf(n-3,2),
                    s3 = 3.0f -2.0f*cosf(2.0f*float(M_PI)/float(n)) - cosf(2.0f*float(M_PI)/float(nm));

                for (int k=0, ofs=vofs; k<length; ++k, ++ofs) {
                    Ep[ofs] = opos[ofs] + e0[ofs] * csf(n-3, 2*start) + e1[ofs]*csf(n-3, 2*start +1);
                    Em[ofs] = opos[ofs] + e0[ofs] * csf(n-3, 2*prev ) + e1[ofs]*csf(n-3, 2*prev + 1);
                    Fp[ofs] = (csf(np-3,2)*opos[ofs] + s1*Ep[ofs] + s2*Em_ip[k] + rp[start*length+k])/3.0f;
                    Fm[ofs] = (csf(nm-3,2)*opos[ofs] + s3*Em[ofs] + s2*Ep_im[k] - rp[prev*length+k])/3.0f;
                }
            } else if (valences[vid] < -2) {
                unsigned int jp = (ivalence + start - zerothNeighbors[vid]) % ivalence,
                    jm = (ivalence + prev  - zerothNeighbors[vid]) % ivalence;

                float s1 = 3-2*csf(n-3,2)-csf(np-3,2),
                    s2 = 2*csf(n-3,2),
                    s3 = 3.0f-2.0f*cosf(2.0f*float(M_PI)/n)-cosf(2.0f*float(M_PI)/nm);

                for (int k=0, ofs=vofs; k<length; ++k, ++ofs) {
                    Ep[ofs] = opos[ofs] + cosf((float(M_PI)*jp)/float(ivalence-1))*e0[ofs] + sinf((float(M_PI)*jp)/float(ivalence-1))*e1[ofs];
                    Em[ofs] = opos[ofs] + cosf((float(M_PI)*jm)/float(ivalence-1))*e0[ofs] + sinf((float(M_PI)*jm)/float(ivalence-1))*e1[ofs];
                    Fp[ofs] = (csf(np-3,2)*opos[ofs] + s1*Ep[ofs] + s2*Em_ip[k] + rp[start*length+k])/3.0f;
                    Fm[ofs] = (csf(nm-3,2)*opos[ofs] + s3*Em[ofs] + s2*Ep_im[k] - rp[prev*length+k])/3.0f;
                }

                if (valences[im]<0) {
                    s1=3-2*csf(n-3,2)-csf(np-3,2);
                    for (int k=0, ofs=vofs; k<length; ++k, ++ofs) {
                        Fp[ofs] = Fm[ofs] = (csf(np-3,2)*opos[ofs] + s1*Ep[ofs] + s2*Em_ip[k] + rp[start*length+k])/3.0f;
                    }
                } else if (valences[ip]<0) {
                    s1 = 3.0f-2.0f*cosf(2.0f*float(M_PI)/n)-cosf(2.0f*float(M_PI)/nm);
                    for (int k=0, ofs=vofs; k<length; ++k, ++ofs) {
                        Fm[ofs] = Fp[ofs] = (csf(nm-3,2)*opos[ofs] + s1*Em[ofs] + s2*Ep_im[k] - rp[prev*length+k])/3.0f;
                    }
                }
            } else if (valences[vid]==-2) {
                for (int k=0, ofs=vofs, ipofs=ip*length, imofs=im*length; k<length; ++k, ++ofs, ++ipofs, ++imofs) {
                    Ep[ofs] = (2.0f * org[ofs] + org[ipofs])/3.0f;
                    Em[ofs] = (2.0f * org[ofs] + org[imofs])/3.0f;
                    Fp[ofs] = Fm[ofs] = (4.0f * org[ofs] + org[((vid+2)%n)*stride+k] + 2.0f * org[ipofs] + 2.0f * org[imofs])/9.0f;
                }
            }
        }

        float * p[20];
        for (int vid=0, ofs=0; vid<4; ++vid, ofs+=length) {
            p[vid*5+0] = opos + ofs;
            p[vid*5+1] =   Ep + ofs;
            p[vid*5+2] =   Em + ofs;
            p[vid*5+3] =   Fp + ofs;
            p[vid*5+4] =   Fm + ofs;
        }

        float u = 0.5f, v = 0.5f;
        float U = 1-u, V=1-v;
        float d11 = u+v; if(u+v==0.0f) d11 = 1.0f;
        float d12 = U+v; if(U+v==0.0f) d12 = 1.0f;
        float d21 = u+V; if(u+V==0.0f) d21 = 1.0f;
        float d22 = U+V; if(U+V==0.0f) d22 = 1.0f;

        for (int k=0; k<length; ++k) {
            q[ 5*length+k] = (u*p[ 3][k] + v*p[ 4][k])/d11;
            q[ 6*length+k] = (U*p[ 9][k] + v*p[ 8][k])/d12;
            q[ 9*length+k] = (u*p[19][k] + V*p[18][k])/d21;
            q[10*length+k] = (U*p[13][k] + V*p[14][k])/d22;
        }

        memcpy(q+ 0*length, p[ 0], length*sizeof(float));
        memcpy(q+ 1*length, p[ 1], length*sizeof(float));
        memcpy(q+ 2*length, p[ 7], length*sizeof(float));
        memcpy(q+ 3*length, p[ 5], length*sizeof(float));
        memcpy(q+ 4*length, p[ 2], length*sizeof(float));
        memcpy(q+ 7*length, p[ 6], length*sizeof(float));
        memcpy(q+ 8*length, p[16], length*sizeof(float));
        memcpy(q+11*length, p[12], length*sizeof(float));
        memcpy(q+12*length, p[15], length*sizeof(float));
        memcpy(q+13*length, p[17], length*sizeof(float));
        memcpy(q+14*length, p[11], length*sizeof(float));
        memcpy(q+15*length, p[10], length*sizeof(float));

        if (badPatch) {
            memset(q, 0, 16*3*sizeof(float));
        }

        for (int x = 0; x < 4; ++x) {
            for (int y = 0; y < 4; ++y) {
                int index = y*4 + x;
                bezierVertices.push_back(q[index*3+0]);
                bezierVertices.push_back(q[index*3+1]);
                bezierVertices.push_back(q[index*3+2]);
            }
        }

#if 0
        cpIndices.push_back(patchTables->GetPatchTable()[parray.GetVertIndex() + patchIndex*4 + 0]);
        cpIndices.push_back(patchTables->GetPatchTable()[parray.GetVertIndex() + patchIndex*4 + 1]);
        cpIndices.push_back(patchTables->GetPatchTable()[parray.GetVertIndex() + patchIndex*4 + 2]);
        cpIndices.push_back(patchTables->GetPatchTable()[parray.GetVertIndex() + patchIndex*4 + 3]);
#else
        cpIndices.push_back(-1);
        cpIndices.push_back(-1);
        cpIndices.push_back(-1);
        cpIndices.push_back(-1);
#endif
    }

    return patchTables->GetNumPatches(array);
}
