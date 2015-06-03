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
#include "convert_bezier.h"
#include "bezier/bezier.h"
#include "../common/patchColors.h"

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

void
Mesh::BezierConvert(const float *vertices,
                    OpenSubdiv::Far::TopologyRefiner const *refiner,
                    OpenSubdiv::Far::PatchTable const *patchTable,
                    bool watertight)
{
    using namespace OpenSubdiv;
    using namespace OsdBezier;

    int numTotalPatches = 0;
    _bezierVertices.clear();
    _colors.clear();
    _sharpnesses.clear();
    _wcpFlags.clear();
    _materialIDs.clear();

    std::vector<int> cpIndices; // 16 * numPatches
    std::vector<Far::PatchDescriptor> patchDescs;
    Far::PatchParamTable patchParam;

    // iterate patch types.
    for (int array=0; array < patchTable->GetNumPatchArrays(); ++array) {

        int numPatches = 0;
        Far::PatchDescriptor desc = patchTable->GetPatchArrayDescriptor(array);
        VERBOSE("TransitionType = %d\n", desc.GetPattern());

        if (desc.GetType() != Far::PatchDescriptor::REGULAR) continue;

        numPatches = convertRegular(_bezierVertices, cpIndices,
                                    &vertices[0], patchTable, array);

        numTotalPatches += numPatches;

        // stamp out color and desc
        {
            const float *color = getAdaptivePatchColor(desc);
            for (int i = 0; i < numPatches; ++i) {
                float r = color[0];
                float g = color[1];
                float b = color[2];
                _colors.push_back(r);
                _colors.push_back(g);
                _colors.push_back(b);

                patchDescs.push_back(desc);
            }
        }

        if (false) {
        //if (desc.GetType() == Far::PatchDescriptor::SINGLE_CREASE) {
            // duplicate patchparam
            Far::ConstPatchParamArray srcPatchParam = patchTable->GetPatchParams(array);
            for (int i = 0; i < numPatches/2; ++i) {
                patchParam.push_back(srcPatchParam[i]);
                patchParam.push_back(srcPatchParam[i]);
                //int sharpid = patchTable->GetSharpnessIndexTable()[it->GetPatchIndex() + i];
//                int sharpid = patchTable->GetSharpnessIndices(array)[i];
//                float sharpness = patchTable->GetSharpnessValues()[sharpid];
//                _sharpnesses.push_back(sharpness);
//                _sharpnesses.push_back(-sharpness);
            }
        } else {
            Far::ConstPatchParamArray srcPatchParam = patchTable->GetPatchParams(array);
            for (int i = 0; i < numPatches; ++i) {
                patchParam.push_back(srcPatchParam[i]);
                _sharpnesses.push_back(0);
            }
        }
    }

    printf("# patches = %d\n", numTotalPatches);

    _wcpFlags.resize(numTotalPatches);

    // vertex position verification pass
    std::map<Edge, Bezier> edgeBeziers;
    if (watertight) {

        // save wcp flags
        for (int i = 0; i < numTotalPatches; ++i) {
            //int face = patchParam[i].faceIndex;
            int wcpFlag = 0;
            int rots = 0;//patchDescs[i].rotation??
            //int rots = face->_adaptiveFlags.rots;

            // wcpFlag = patchParam.GetTransitionMask;

            _wcpFlags[i] = wcpFlag;
        }
    }

    // FIXME! we need this mapping for watertight
    std::vector<int> patchIndexToFaceIndex;
    patchIndexToFaceIndex.resize(numTotalPatches);

#if 0
    if (false) {
    //if (watertight) {
        for (int i = 0; i < numTotalPatches; ++i) {
            int parentQuad[] = { 0, 2, 1, 3 };

            int edgeVerts[][4] = { {0, 1, 2, 3}, {0, 4, 8, 12}, {3, 7, 11, 15}, {12, 13, 14, 15} };
            int edgeParents[][2] = { {0, 2}, {0, 1}, {2, 3}, {1, 3} };

            // store bezier edges (skip gregory, single-crease)
            if (patchDescs[i].GetType() == Far::PatchDescriptor::SINGLE_CREASE or
                patchDescs[i].GetType() == Far::PatchDescriptor::GREGORY or
                patchDescs[i].GetType() == Far::PatchDescriptor::GREGORY_BOUNDARY) continue;

            VERBOSE("\n============ patch %d ==============\n", patchParam[i].faceIndex);

            for (int j = 0; j < 4; ++j) {
                int parent = cpIndices[i*4+parentQuad[j]];
                if (parent < 0) continue; // skip border, boundary (for now)
                // parent mapping
                // parent = vertexParentIDs[parent];
                //NO_HBRparent = farToHbr[parent];
                //VERBOSE("%d  %d: %f %f %f\n", i, parent, v[0], v[1], v[2]);

                // for edge verts
                vec3f e0(&_bezierVertices[(i*16 + edgeVerts[j][0]) * 3]);
                vec3f e1(&_bezierVertices[(i*16 + edgeVerts[j][1]) * 3]);
                vec3f e2(&_bezierVertices[(i*16 + edgeVerts[j][2]) * 3]);
                vec3f e3(&_bezierVertices[(i*16 + edgeVerts[j][3]) * 3]);

                int ep0 = cpIndices[i*4 + edgeParents[j][0]];
                int ep1 = cpIndices[i*4 + edgeParents[j][1]];
                Edge edge(ep0, ep1);
                VERBOSE("(%d, %d) (%f, %f, %f)-(%f, %f, %f)\n", ep0, ep1,
                       e0[0], e0[1], e0[2],
                       e3[0], e3[1], e3[2]);
                if (edgeBeziers.count(edge) == 0) {
                    if (edge._edges[0] == ep0) {
                        edgeBeziers[edge] = Bezier(e0, e1, e2, e3);
                    } else {
                        edgeBeziers[edge] = Bezier(e3, e2, e1, e0);
                    }
                }
            }
        }
        /*
             pattern0        pattern1       pattern2        pattern3        pattern4
          +-------------+ +-------------+ +-------------+ +-------------+ +-------------+
          |     /\\     | | 0   /\\   2 | |             | |      |      | |      |      |
          | 1  /  \\  2 | |    /   \\   | |      0      | |  1   |  0   | |      |      |
          |   /    \\   | |   /  3   \\ | |-------------| |------|------| |  1   |   0  |
          |  /      \\  | |  /       /  | |\\    3    / | |      |      | |      |      |
          | /    0   \\ | | /    /    1 | |  \\     /   | |  3   |  2   | |      |      |
          |/          \\| |/ /          | | 1  \\ /   2 | |      |      | |      |      |
          +-------------+ +-------------+ +-------------+ +-------------+ +-------------+
        */
        std::map<std::pair<int, int>, int> finalFaces;
        for (int i = 0; i < numTotalPatches; ++i) {
            int faceIndex = patchIndexToFaceIndex[i];
            //int faceIndex = patchParam[i].vtrFaceIndex;
            int level = patchParam[i].bitField.GetDepth();
            finalFaces[std::make_pair(level, faceIndex)] = i; // what if single-crease?
        }

        // different level subdivision
        // water-tight critical
        for (int i = 0; i < numTotalPatches; ++i) {
            //int hbrFace = patchParam[i].hbrFaceIndex;
            //OsdHbrFace *face = hbrMesh->GetFace(hbrFace);
            //int faceIndex = patchParam[i].vtrFaceIndex;
            int faceIndex = patchIndexToFaceIndex[i];

            if (patchDescs[i].GetType() == Far::PatchDescriptor::SINGLE_CREASE) continue;
            // ---------------- gregory
            //   0-----3
            //   |     |
            //   |     |
            //   1-----2
            if (patchDescs[i].GetType() == Far::PatchDescriptor::GREGORY or
                patchDescs[i].GetType() == Far::PatchDescriptor::GREGORY_BOUNDARY) {
                int edgeParents[][2] = { {0, 1}, {1, 2}, {2, 3}, {3, 0} };
                for (int j = 0; j < 4; ++j) {
                    int v0 = cpIndices[i*4 + edgeParents[j][0]];
                    int v1 = cpIndices[i*4 + edgeParents[j][1]];
                    Edge e(v0, v1);
                    if (edgeBeziers.count(e) != 0) {
                        // overwrite
                        Bezier edge = edgeBeziers[e];
                        bool reverse = e._edges[0] != v0;
                        vec3f *d = (vec3f*)(&_bezierVertices[i*16*3]);
                        if (j == 0) {
                            for (int k = 0; k < 4; ++k) {
                                d[k*4+0] = reverse ? edge.cp[3-k] : edge.cp[k];
                            }
                        } else if (j == 1) {
                            for (int k = 0; k < 4; ++k) {
                                d[12+k] = reverse ? edge.cp[3-k] : edge.cp[k];
                            }
                        } else if (j == 2) {
                            for (int k = 0; k < 4; ++k) {
                                d[k*4+3] = reverse ? edge.cp[k] : edge.cp[3-k];
                            }
                        } else if (j == 3) {
                            for (int k = 0; k < 4; ++k) {
                                d[k] = reverse ? edge.cp[k] : edge.cp[3-k];
                            }
                        }
                    }
                }
                continue;
            }

            // ---------------- non gregory
            //if (_wcpFlags[i] == 0) continue;
            VERBOSE("\n");

            int level = patchParam[i].bitField.GetDepth()-1;
            if (level < 0) continue;
            //int parentFace = refiner->GetChildFaceParentFace(level, faceIndex);
            int parentFace = refiner->GetFaceParentFace(level+1, faceIndex);
            Vtr::ConstIndexArray indices = refiner->GetFaceVertices(level, parentFace);

            VERBOSE("Level %d : \e[32mPatch %d\e[0m, parent = %d (Flag=%d)----\n",
                    level+1, faceIndex, parentFace, _wcpFlags[i]);

            Vtr::ConstIndexArray myEdges = refiner->GetFaceEdges(level+1, faceIndex);
            VERBOSE("My edges verts:");
            for (int i = 0; i < myEdges.size(); ++i) {
                VERBOSE(" %d ", myEdges[i]);
            }
            VERBOSE("\n");

            VERBOSE("Parent verts: level(%d)= ", level);
            for (int i = 0; i < indices.size(); ++i) {
                VERBOSE(" %d ", indices[i]);
            }
            VERBOSE("\n");

            if (indices.size() != 4) {
                VERBOSE("non-quad parent (nverts=%d)\n", (int)indices.size());
                continue;
            }

            // locate which child this face is, in the parent face (within 0-3)

            int childIndex = -1;
            bool watertightEdges[4] = { false, false, false, false };
            Vtr::ConstIndexArray children = refiner->GetFaceChildFaces(level, parentFace);
            Vtr::ConstIndexArray edges = refiner->GetFaceEdges(level, parentFace);

            for (int j = 0; j < 4; ++j) {
                if (children[j] == faceIndex) childIndex = j;
            }
            //int rot = patchDescs[i].GetRotation();
            int rot = patchParam[i].bitField.GetRotation();
            VERBOSE(" CHILD=%d, \e[31mrot=%d\e[0m\n", childIndex, rot);//face->_adaptiveFlags.rots);
            if (childIndex == -1) continue;

            VERBOSE("Child faces: (childIndex = %d)", childIndex);
            for (int i = 0; i < children.size(); ++i) {
                VERBOSE(" %d ", children[i]);
            }
            VERBOSE("\n");

            // check 2 edges.
            for (int j = 0; j < 2; ++j) {
                int edgeIndex = (j+3+childIndex)%4;

                //edgeIndex = (edgeIndex + rot)%4;

                if (patchDescs[i].GetType() == Far::PatchDescriptor::BOUNDARY or
                    patchDescs[i].GetType() == Far::PatchDescriptor::CORNER) continue;

                // if it's triangle head, skip
                //if ((_wcpFlags[i] >> edgeIndex)&1 == 1) continue;
                //if (_wcpFlags[i]) continue;

                Vtr::ConstIndexArray faces = refiner->GetEdgeFaces(level, edges[edgeIndex]);
                if (faces.size() != 2) continue;
                int adjFace = (faces[0] == parentFace) ? faces[1] : faces[0];
                VERBOSE("Adjacent face %d\n", adjFace);

                // see if the adjacent face is final or not
                if (finalFaces.count(std::make_pair(level, adjFace)) > 0) {
                    VERBOSE("Critical Patch (level=%d, adjFace=%d exists)!\n", level, adjFace);
                    watertightEdges[edgeIndex] = true;
                } else {
                    continue;
                }

                //int patchIndex = finalFaces[std::make_pair(level, adjFace)];


                // inefficient!
                int levelVertsOffset = 0;
                for (int k = 0; k < level; ++k){
                    levelVertsOffset += refiner->GetNumVertices(k);
                }
                int v0 = indices[edgeIndex] + levelVertsOffset;
                int v1 = indices[(edgeIndex+1)%4] + levelVertsOffset;
                VERBOSE("Matching verts: (level %d), %d- %d,  edge = %d\n", level,
                       v0, v1, edges[edgeIndex]);

                Edge e(v0, v1);

                //if (parentFace->_adaptiveFlags.bverts > 0) continue;
                // childindex 0 -> edge 3, 0
                // childindex 1 -> edge 0, 1
                // childindex 2 -> edge 1, 2
                // childindex 3 -> edge 2, 3
                // for each edge

                    /*
                      <----------- v
                          edge 0
                      +-----+-----+         u
                      |     |     |         |
                      |  1  |  0  |         |
                      |     |     |         |
               edge 1 +-----+-----+ edge 3  |
                      |     |     |         |
                      |  2  |  3  |         v
                      |     |(childIndex)|
                      +-----+-----+
                          edge 2
                     */

                if (edgeBeziers.count(e) != 0) {
                    Bezier parentEdge = edgeBeziers[e];  // todo, fix lookup
                    bool reverse = e._edges[0] != v0;
                    if (reverse) {
                        parentEdge.Reverse();
                    }

                    // apply rotation (for transition patches)
                    int edge = edgeIndex;
                    edge = (edge+(4-rot))%4;

                    VERBOSE("Align edgeIndex = %d, edge = %d\n", edgeIndex, edge);

                    // find corresponding edge at patch[i]
                    // cut original bezier
                    vec3f tmp[2][4];
                    OsdBezier::BezierSplit<4, 0, /*stride*/1, vec3f, float> split(
                        tmp[0], tmp[1], edgeBeziers[e].cp, 0.5);
                    VERBOSE("[[%f %f %f - ", tmp[0][0][0], tmp[0][0][1], tmp[0][0][2]);
                    VERBOSE("%f %f %f - ", tmp[1][0][0], tmp[1][0][1], tmp[1][0][2]);
                    VERBOSE("%f %f %f]]\n", tmp[1][3][0], tmp[1][3][1], tmp[1][3][2]);

                    vec3f *d = (vec3f*)(&_bezierVertices[i*16*3]);
                    if (edge == 0) {
                        for (int k = 0; k < 4; ++k) {
                            d[k*4+0] = reverse
                                ? tmp[childIndex==edgeIndex?1:0][3-k]
                                : tmp[childIndex==edgeIndex?0:1][k];
                        }
                    } else if (edge == 1) {
                        for (int k = 0; k < 4; ++k) {
                            d[12+k] = reverse
                                ? tmp[childIndex==edgeIndex?1:0][3-k]
                                : tmp[childIndex==edgeIndex?0:1][k];
                        }
                    } else if (edge == 2) {
                        for (int k = 0; k < 4; ++k) {
                            d[k*4+3] = reverse
                                ? tmp[childIndex==edgeIndex?1:0][k]
                                : tmp[childIndex==edgeIndex?0:1][3-k];
                        }
                    } else if (edge == 3) {
                        for (int k = 0; k < 4; ++k) {
                            d[k] = reverse
                                ? tmp[childIndex==edgeIndex?1:0][k]
                                : tmp[childIndex==edgeIndex?0:1][3-k];
                        }
                    }
                } else {
                    // not found in the edge dictionary.
                    //printf("not found. patch %d\n", i);
                }
            }
        }
    }
#endif

    _numTriangles = 0;
    _numBezierPatches = numTotalPatches;
    _patchParams = patchParam; // for pre-tessellation

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
        int ptexIndex = _patchParams[i].faceIndex;
        int faceIndex = ptexIDToFaceIDMapping[ptexIndex];
        int materialID = materialBinds[faceIndex];
        //printf("%d->%d\n", faceIndex, materialID);
        _materialIDs[i] = materialID;
    }
}

static void evalBezier(float *p, float *n, float u, float v, const float *cp)
{
    OsdBezier::BezierPatch<OsdBezier::vec3f, float, 4> patch((const OsdBezier::vec3f*)cp);
    OsdBezier::vec3f b = patch.Evaluate(u, v);
    p[0] = b[0];
    p[1] = b[1];
    p[2] = b[2];
    OsdBezier::vec3f du = patch.EvaluateDu(u, v);
    OsdBezier::vec3f dv = patch.EvaluateDv(u, v);
    n[0] = -(du[1] * dv[2] - du[2] * dv[1]);
    n[1] = -(du[2] * dv[0] - du[0] * dv[2]);
    n[2] = -(du[0] * dv[1] - du[1] * dv[0]);
}

void
Mesh::Tessellate(int level)
{
    _triVertices.clear();
    _triNormals.clear();
    _faces.clear();
    _numTriangles = 0;
    std::vector<float> colors;

    if (level == 0) {
        return;
    }

    float *cp = &_bezierVertices[0];
    int vindex = 0;
    int numTriangles = 0;

    for (size_t patchIndex = 0; patchIndex < _numBezierPatches; ++patchIndex) {

        const OpenSubdiv::Far::PatchParam &param = _patchParams[patchIndex];
        unsigned int bits = param.bitField.field;
        int patchLevel = (bits & 0xf);
        int div = 1 << std::max(0, level-patchLevel);
        int udiv = div;
        int vdiv = div;

        float sharp = _sharpnesses[patchIndex];
        if (sharp > 0) {
            udiv = 1 << std::max(0, level-patchLevel-int(sharp));
        } else if (sharp < 0) {
            udiv = div - (1 << std::max(0, level-patchLevel+int(sharp)));
            udiv = std::max(1, udiv);
        }

        float p[3], n[3];
        for (int u = 0; u <= udiv; ++u) {
            for (int v = 0; v <= vdiv; ++v) {
                evalBezier(p, n, u/float(udiv), v/float(vdiv), cp);
                _triVertices.push_back(p[0]);
                _triVertices.push_back(p[1]);
                _triVertices.push_back(p[2]);
                _triNormals.push_back(n[0]);
                _triNormals.push_back(n[1]);
                _triNormals.push_back(n[2]);

                if (u < udiv and v < vdiv) {
                    _faces.push_back(vindex);
                    _faces.push_back(vindex+1);
                    _faces.push_back(vindex+vdiv+1);

                    _faces.push_back(vindex+vdiv+1);
                    _faces.push_back(vindex+1);
                    _faces.push_back(vindex+vdiv+2);

                    colors.push_back(_colors[patchIndex*3+0]);
                    colors.push_back(_colors[patchIndex*3+1]);
                    colors.push_back(_colors[patchIndex*3+2]);
                    colors.push_back(_colors[patchIndex*3+0]);
                    colors.push_back(_colors[patchIndex*3+1]);
                    colors.push_back(_colors[patchIndex*3+2]);
                    numTriangles += 2;
                }
                ++vindex;
            }
        }

        // next
        cp += 16*3;
    }

    _numTriangles = numTriangles;
    _colors.swap(colors);
}
