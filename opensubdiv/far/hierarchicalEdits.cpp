//
//   Copyright 2015 Pixar
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
#include "../far/hierarchicalEdits.h"
#include "../far/topologyRefiner.h"

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Far {

void
HierarchicalEdits::IndexPath::Resolve(
    OpenSubdiv::Far::TopologyRefiner const *refiner, int level)
{
    //
    // resolve face-local indices to level-local indices.
    // note that the last localindex could be vert/edge index.

    while (indicesInLevel.size() < (size_t)level) {
        // XXX: revisit here when we'll add hierarchical face edit (no -1).
        int maxLevel = indicesInLevel.size()-1;
        int parentFace = indicesInLevel[maxLevel];
        int localIndex = indicesInFace[maxLevel];
        OpenSubdiv::Far::ConstIndexArray childFaces =
            refiner->GetFaceChildFaces(maxLevel, parentFace);
        int childFace = childFaces[localIndex];
        indicesInLevel.push_back(childFace);
    }
}

void
HierarchicalEdits::ApplyHierarchicalSharpnessEdit(
    int level, OpenSubdiv::Far::TopologyRefiner *refiner) {

    // apply edge sharpness edit
    for (size_t i = 0; i < edgeSharpnessEdits.size(); ++i) {
        IndexPath &edit = edgeSharpnessEdits[i];
        if (level+1 != (int)edit.indicesInFace.size()) continue;

        refiner->SetEdgeSharpness(level,
                                  edit.indicesInLevel[level+1],
                                  edgeSharpnesses[i]);
    }

    // apply vertex sharpness edit
    for (size_t i = 0; i < vertexSharpnessEdits.size(); ++i) {
        IndexPath &edit = vertexSharpnessEdits[i];
        if (level+1 != (int)edit.indicesInFace.size()) continue;

        refiner->SetVertexSharpness(level,
                                    edit.indicesInLevel[level+1],
                                    vertexSharpnesses[i]);
    }
}

void
HierarchicalEdits::ResolveIndexPath(
    int level, OpenSubdiv::Far::TopologyRefiner *refiner) {

    for (size_t i = 0; i < vertexValueEdits.size(); ++i) {
        IndexPath &edit = vertexValueEdits[i];

        // we're at higher level than the edit level.
        if (level >= (int)edit.indicesInFace.size()) continue;

        // resolve face-local indices to level-local indices.
        edit.Resolve(refiner, level+1);

        // last level
        if (level == (int)edit.indicesInFace.size()-1) {
            OpenSubdiv::Far::ConstIndexArray vindices =
                refiner->GetFaceVertices(level, edit.indicesInLevel[level]);
            edit.indicesInLevel.push_back(vindices[edit.indicesInFace[level]]);
        }
    }

    for (size_t i = 0; i < edgeSharpnessEdits.size(); ++i) {
        IndexPath &edit = edgeSharpnessEdits[i];
        if (level >= (int)edit.indicesInFace.size()) continue;

        edit.Resolve(refiner, level+1);

        // last index is edge.
        if (level == (int)edit.indicesInFace.size()-1) {
            OpenSubdiv::Far::ConstIndexArray edges =
                refiner->GetFaceEdges(level, edit.indicesInLevel[level]);
            edit.indicesInLevel.push_back(edges[edit.indicesInFace[level]]);
        }
    }

    // apply edge sharpness edit
    for (size_t i = 0; i < vertexSharpnessEdits.size(); ++i) {
        IndexPath &edit = vertexSharpnessEdits[i];
        if (level >= (int)edit.indicesInFace.size()) continue;

        edit.Resolve(refiner, level+1);

        // last index is vertex.
        if (level == (int)edit.indicesInFace.size()-1) {
            OpenSubdiv::Far::ConstIndexArray vertices =
                refiner->GetFaceVertices(level, edit.indicesInLevel[level]);
            edit.indicesInLevel.push_back(vertices[edit.indicesInFace[level]]);
        }
    }
}

bool
HierarchicalEdits::IsIsolationRequired(int level, int face) {

    // this function can be optimized more by sorting edits by level.

    // search in creaseedits which have more depth than level
    for (size_t i = 0; i < edgeSharpnessEdits.size(); ++i) {
        IndexPath &edit = edgeSharpnessEdits[i];
        if (level >= (int)edit.indicesInFace.size()) continue; // no more isolation needed

        // need isolation for this face
        if (edit.indicesInLevel[level] == face) {
            return true;
        }
    }
    for (size_t i = 0; i < vertexSharpnessEdits.size(); ++i) {
        IndexPath &edit = vertexSharpnessEdits[i];
        if (level >= (int)edit.indicesInFace.size()) continue;

        // need isolation for this face
        if (edit.indicesInLevel[level] == face) {
            return true;
        }
    }
    for (size_t i = 0; i < vertexValueEdits.size(); ++i) {
        IndexPath &edit = vertexValueEdits[i];
        if (level >= (int)edit.indicesInFace.size()) continue;

        // need isolation for this face
        if (edit.indicesInLevel[level] == face) {
            return true;
        }
    }
    return false;
}

} // end namespace Far

} // end namespace OPENSUBDIV_VERSION
} // end namespace OpenSubdiv
