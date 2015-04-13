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
#ifndef FAR_HIERARCHICAL_EDITS_H
#define FAR_HIERARCHICAL_EDITS_H

#include "../version.h"

#include <vector>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Far {

class TopologyRefiner;

struct HierarchicalEdits {

    struct IndexPath {
        // hedit tag (rootface, localface, localface, ... localface/localvert/localedge )

        // local indices
        std::vector<int> indicesInFace;
        // resolve face-local indices to level-local indices
        std::vector<int> indicesInLevel;

        // resolve localIndices (face-local) to indices (level-local)
        void Resolve(TopologyRefiner const *refiner, int level);
    };

    bool IsEmpty() const {
        return vertexValueEdits.empty() and
            vertexSharpnessEdits.empty() and
            edgeSharpnessEdits.empty();
    }

    void ApplyHierarchicalSharpnessEdit(int level, TopologyRefiner *refiner);

    bool IsIsolationRequired(int level, int face);

    void ResolveIndexPath(int level, TopologyRefiner *refiner);

    std::vector<IndexPath> vertexValueEdits;
    std::vector<IndexPath> vertexSharpnessEdits;
    std::vector<IndexPath> edgeSharpnessEdits;
    std::vector<float> vertexSharpnesses;
    std::vector<float> edgeSharpnesses;
};

} // end namespace Far

} // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;
} // end namespace OpenSubdiv


#endif  // FAR_HIERARCHICAL_EDITS_H
