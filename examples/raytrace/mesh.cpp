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

#include <cfloat>
#include <set>
#include <map>
#include "mesh.h"
#include "bezier/bezier.h"

#define VERBOSE(x, ...)
//#define VERBOSE printf

struct Edge {
    Edge(int a, int b) {
        if (a > b) std::swap(a, b);
        _edges[0] = a;
        _edges[1] = b;
    }
    bool operator < (Edge const &other) const {
        return (_edges[0] < other._edges[0] ||
                (_edges[0] == other._edges[0] && _edges[1]  < other._edges[1]));
    }
    int _edges[2];
};

struct Bezier {
    Bezier() {}
    Bezier(OsdBezier::vec3f p[4]) {
        cp[0] = p[0];
        cp[1] = p[1];
        cp[2] = p[2];
        cp[3] = p[3];
    }
    Bezier(OsdBezier::vec3f const &p0, OsdBezier::vec3f const &p1,
           OsdBezier::vec3f const &p2, OsdBezier::vec3f const &p3) {
        cp[0] = p0;
        cp[1] = p1;
        cp[2] = p2;
        cp[3] = p3;
    }
    void Reverse() {
        std::swap(cp[0], cp[3]);
        std::swap(cp[1], cp[2]);
    }
    OsdBezier::vec3f cp[4];
};

inline int bitcount(int i)
{
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

static const float *getAdaptivePatchColor(OpenSubdiv::Far::PatchParam const &patchParam, float sharpness)
{
    static float _colors[][4] = {
        {1.0f,  1.0f,  1.0f,  1.0f},   // regular
        {0.0f,  1.0f,  1.0f,  1.0f},   // regular pattern 0
        {0.0f,  0.5f,  1.0f,  1.0f},   // regular pattern 1
        {0.0f,  0.5f,  0.5f,  1.0f},   // regular pattern 2
        {0.5f,  0.0f,  1.0f,  1.0f},   // regular pattern 3
        {1.0f,  0.5f,  1.0f,  1.0f},   // regular pattern 4

        {1.0f,  0.5f,  0.5f,  1.0f},   // single crease
        {1.0f,  0.70f,  0.6f,  1.0f},  // single crease pattern 0
        {1.0f,  0.65f,  0.6f,  1.0f},  // single crease pattern 1
        {1.0f,  0.60f,  0.6f,  1.0f},  // single crease pattern 2
        {1.0f,  0.55f,  0.6f,  1.0f},  // single crease pattern 3
        {1.0f,  0.50f,  0.6f,  1.0f},  // single crease pattern 4

        {0.8f,  0.0f,  0.0f,  1.0f},   // boundary
        {0.0f,  0.0f,  0.75f, 1.0f},   // boundary pattern 0
        {0.0f,  0.2f,  0.75f, 1.0f},   // boundary pattern 1
        {0.0f,  0.4f,  0.75f, 1.0f},   // boundary pattern 2
        {0.0f,  0.6f,  0.75f, 1.0f},   // boundary pattern 3
        {0.0f,  0.8f,  0.75f, 1.0f},   // boundary pattern 4

        {0.0f,  1.0f,  0.0f,  1.0f},   // corner
        {0.25f, 0.25f, 0.25f, 1.0f},   // corner pattern 0
        {0.25f, 0.25f, 0.25f, 1.0f},   // corner pattern 1
        {0.25f, 0.25f, 0.25f, 1.0f},   // corner pattern 2
        {0.25f, 0.25f, 0.25f, 1.0f},   // corner pattern 3
        {0.25f, 0.25f, 0.25f, 1.0f},   // corner pattern 4

        {1.0f,  1.0f,  0.0f,  1.0f},   // gregory
        {1.0f,  1.0f,  0.0f,  1.0f},   // gregory
        {1.0f,  1.0f,  0.0f,  1.0f},   // gregory
        {1.0f,  1.0f,  0.0f,  1.0f},   // gregory
        {1.0f,  1.0f,  0.0f,  1.0f},   // gregory
        {1.0f,  1.0f,  0.0f,  1.0f},   // gregory

        {1.0f,  0.5f,  0.0f,  1.0f},   // gregory boundary
        {1.0f,  0.5f,  0.0f,  1.0f},   // gregory boundary
        {1.0f,  0.5f,  0.0f,  1.0f},   // gregory boundary
        {1.0f,  0.5f,  0.0f,  1.0f},   // gregory boundary
        {1.0f,  0.5f,  0.0f,  1.0f},   // gregory boundary
        {1.0f,  0.5f,  0.0f,  1.0f},   // gregory boundary

        {1.0f,  0.7f,  0.3f,  1.0f},   // gregory basis
        {1.0f,  0.7f,  0.3f,  1.0f},   // gregory basis
        {1.0f,  0.7f,  0.3f,  1.0f},   // gregory basis
        {1.0f,  0.7f,  0.3f,  1.0f},   // gregory basis
        {1.0f,  0.7f,  0.3f,  1.0f},   // gregory basis
        {1.0f,  0.7f,  0.3f,  1.0f}};  // gregory basis

    int patchType = 0;
    int boundary = patchParam.GetBoundary();
    if (bitcount(boundary) == 1) {
        patchType = 2;
    } else if (bitcount(boundary) == 2) {
        patchType = 3;
    }
    if (sharpness > 0) {
        patchType = 1;
    }

    int pattern = bitcount(patchParam.GetTransition());
    
    return _colors[6*patchType + pattern];
}
#include "bezier/math.h"
#include "bezier/bezier.h"
#include "common.h"

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

// flip matrix orientation
inline matrix4f
flipMatrix(matrix4f m)
{
    return matrix4f(m[3][3], m[3][2], m[3][1], m[3][0],
                    m[2][3], m[2][2], m[2][1], m[2][0],
                    m[1][3], m[1][2], m[1][1], m[1][0],
                    m[0][3], m[0][2], m[0][1], m[0][0]);
}


int
convertRegular(std::vector<float> &bezierVertices,
               float const *vertices,
               OpenSubdiv::Far::PatchTable const *patchTable,
               int array, int patch, float sharpness)
{
    // regular to bezier conversion
    const int *verts = &patchTable->GetPatchArrayVertices(array)[patch*16];

    // reflect boundary mask verts
    OpenSubdiv::Far::PatchParam param = patchTable->GetPatchParams(array)[patch];
    int boundaryMask = param.GetBoundary();

    // BSpline to Bezier matrix
    matrix4f Q(1.f/6.f, 4.f/6.f, 1.f/6.f, 0.f,
               0.f,     4.f/6.f, 2.f/6.f, 0.f,
               0.f,     2.f/6.f, 4.f/6.f, 0.f,
               0.f,     1.f/6.f, 4.f/6.f, 1.f/6.f);

    // Infinitely Sharp matrix (boundary)
    matrix4f Mi(1.f/6.f, 4.f/6.f, 1.f/6.f, 0.f,
                0.f,     4.f/6.f, 2.f/6.f, 0.f,
                0.f,     2.f/6.f, 4.f/6.f, 0.f,
                0.f,     0.f,     1.f,     0.f);

    matrix4f Mj, Ms;
    if (sharpness > 0) {
        float Sf = floor(sharpness);
        float Sc = ceil(sharpness);
        float Sr = sharpness - Sf;
        matrix4f Mf = computeMatrixSimplified(Sf);
        matrix4f Mc = computeMatrixSimplified(Sc);
        Mj = Mf * (1-Sr) + Mi * Sr;
        Ms = Mf * (1-Sr) + Mc * Sr;
    } else {
        Mj = Ms = Mi;
    }

    matrix4f MUi, MUj, MUs;
    matrix4f MVi, MVj, MVs;
    MUi = MUj = MUs = Q;
    MVi = MVj = MVs = Q;

    if ((boundaryMask & 1) != 0) {
        MVi = flipMatrix(Mi);
        MVj = flipMatrix(Mj);
        MVs = flipMatrix(Ms);
    }
    if ((boundaryMask & 2) != 0) {
        MUi = Mi;
        MUj = Mj;
        MUs = Ms;
    }
    if ((boundaryMask & 4) != 0) {
        MVi = Mi;
        MVj = Mj;
        MVs = Ms;
    }
    if ((boundaryMask & 8) != 0) {
        MUi = flipMatrix(Mi);
        MUj = flipMatrix(Mj);
        MUs = flipMatrix(Ms);
    }

    vec3f P[16];
    for (int j = 0; j < 16; ++j) {
        P[j] = vec3f(&vertices[verts[j]*3]);
    }

    vec3f bp[16], bpi[16], bpj[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            vec3f Hi[4], Hj[4], Hs[4];
            for (int l = 0; l < 4; ++l) {
                Hi[l] = Hj[l] = Hs[l] = vec3f(0.0f);
                for (int k = 0; k < 4; ++k) {
                    Hi[l] += P[l*4+k] * MUi[i][k];
                    Hj[l] += P[l*4+k] * MUj[i][k];
                    Hs[l] += P[l*4+k] * MUs[i][k];
                }
            }
            bp[i*4+j] = vec3f(0.0f);
            for (int k=0; k<4; ++k) {
                bp[i*4+j]  += Hi[k] * MVi[j][k];
                bpi[i*4+j] += Hj[k] * MVj[j][k];
                bpj[i*4+j] += Hs[k] * MVs[j][k];
            }
        }
    }

    if (sharpness > 0) {
        // split single-cerase patch into two bezier-patches.
        OsdBezier::BezierPatch<vec3f, float, 4> patch0(bp);
        OsdBezier::BezierPatch<vec3f, float, 4> patch1(bpj);

        OsdBezier::BezierPatch<vec3f, float, 4> patch0s[2];
        OsdBezier::BezierPatch<vec3f, float, 4> patch1s[2];
        float t = pow(2.0f, -ceil(sharpness));
        if ((boundaryMask & 1) != 0) {
            patch0.SplitU(patch0s, t);
            patch1.SplitU(patch1s, t);
        } else if ((boundaryMask & 2) != 0) {
            t = 1 - t;
            patch0.SplitV(patch1s, t);
            patch1.SplitV(patch0s, t);
        } else if ((boundaryMask & 4) != 0) {
            t = 1 - t;
            patch0.SplitU(patch1s, t);
            patch1.SplitU(patch0s, t);
        } else {
            patch0.SplitV(patch0s, t);
            patch1.SplitV(patch1s, t);
        }

        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                bp[j*4+k] = patch0s[1].Get(k, j);
                bpj[j*4+k] = patch1s[0].Get(k, j);
            }
        }
        // TODO: fractional sharpness
        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(bp[j][0]);
            bezierVertices.push_back(bp[j][1]);
            bezierVertices.push_back(bp[j][2]);
        }
        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(bpj[j][0]);
            bezierVertices.push_back(bpj[j][1]);
            bezierVertices.push_back(bpj[j][2]);
        }
        return 2;
    } else {
        for (int j = 0; j < 16; ++j) {
            bezierVertices.push_back(bp[j][0]);
            bezierVertices.push_back(bp[j][1]);
            bezierVertices.push_back(bp[j][2]);
        }
        return 1;
    }
}

void
Mesh::BezierConvert(const float *vertices,
                    OpenSubdiv::Far::PatchTable const *patchTable)
{
    using namespace OpenSubdiv;
    using namespace OsdBezier;

    int numTotalPatches = 0;
    _bezierVertices.clear();
    _colors.clear();
    _patchParams.clear();

    // iterate patch types.
    for (int array=0; array < patchTable->GetNumPatchArrays(); ++array) {

        int numPatches = patchTable->GetNumPatches(array);
        Far::PatchDescriptor desc = patchTable->GetPatchArrayDescriptor(array);

        // Regular B-Spline only.
        if (desc.GetType() != Far::PatchDescriptor::REGULAR) continue;

        for (int i = 0; i < numPatches; ++i) {

            Far::PatchParam const &patchParam = patchTable->GetPatchParam(array, i);

            // sharpness
            float sharpness = 0;
            // XXX: this far API is weird. regular patch has to be the first one.
            int sharpnessIndex = patchTable->GetSharpnessIndexTable()[i];
            if (sharpnessIndex >=0) sharpness = patchTable->GetSharpnessValues()[sharpnessIndex];

            int n = convertRegular(_bezierVertices, vertices, patchTable, array, i, sharpness);

            // save color coding
            const float *color = getAdaptivePatchColor(patchParam, sharpness);
            for (int j = 0; j < n; ++j) {
                _patchParams.push_back(patchParam);
                float r = color[0];
                float g = color[1];
                float b = color[2];
                _colors.push_back(r);
                _colors.push_back(g);
                _colors.push_back(b);
            }

            numTotalPatches += n;
        }
    }

    printf("# patches = %d\n", numTotalPatches);
    _numBezierPatches = numTotalPatches;

    assert(numTotalPatches*16*3 == (int)_bezierVertices.size());
}

void
Mesh::AssignMaterialIDs(std::vector<int> const &ptexIDToFaceIDMapping,
                        std::vector<unsigned short> const &materialBinds)
{
    // resolve material indices
    _materialIDs.clear();
    _materialIDs.resize(_numBezierPatches);

    if (ptexIDToFaceIDMapping.empty() ||
        materialBinds.empty()) return;

    for (size_t i = 0; i < _numBezierPatches; ++i) {
        int ptexIndex = _patchParams[i].GetFaceId();
        int faceIndex = ptexIDToFaceIDMapping[ptexIndex];
        int materialID = materialBinds[faceIndex];
        //printf("%d->%d\n", faceIndex, materialID);
        _materialIDs[i] = materialID;
    }
}
