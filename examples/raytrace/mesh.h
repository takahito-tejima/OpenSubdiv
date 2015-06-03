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
#ifndef OSD_RAYTRACE_MESH_H
#define OSD_RAYTRACE_MESH_H

#include <cstdio>
#include <vector>
#include <far/topologyRefiner.h>
#include <far/patchTable.h>

#include "common.h"

struct Material {
    Material() : diffuse(0.5f), reflection(0.01f), refraction(0.0f),
                 reflectionGlossiness(1.0f), refractionGlossiness(1.0f),
                 fresnel(false), ior(1.0f) { }
    vec3f diffuse;
    vec3f reflection;
    vec3f refraction;
    float reflectionGlossiness;
    float refractionGlossiness;
    bool  fresnel;
    float ior;
};

class Mesh {
public:
    Mesh() : _numBezierPatches(0) { }

    void BezierConvert(const float *vertices,
                       OpenSubdiv::Far::PatchTable const *patchTable);

    void AssignMaterialIDs(std::vector<int> const &ptexIDToFaceIDMapping,
                           std::vector<unsigned short> const &materialBinds);

    int GetNumPatches() const { return _numBezierPatches; }

    const vec3f* GetBezierVerts(int face) const {
        return (const vec3f*)&_bezierVertices[face * 16 * 3];
    }

    Material const &GetMaterial(int matID) const { return _materials[matID]; }
    void SetMaterial(int matID, Material const &mat) {
        if ((int)_materials.size() <= matID) _materials.resize(matID+1);
        _materials[matID] = mat;
    }
    void AssignMaterial(int patchIndex, int matID) {
        _materialIDs[patchIndex] = matID;
    }
    int GetMaterialID(int patchIndex) const {
        return _materialIDs[patchIndex];
    }

    size_t GetMemoryUsage() const {
        // patches memory
        return _bezierVertices.size() * sizeof(float);  // cp
    }

    vec3f GetColor(int face) const { return vec3f(&_colors[face*3]); }
    OpenSubdiv::Far::PatchParam GetPatchParam(int face) const
        { return _patchParams[face]; }

private:
    // patches
    size_t _numBezierPatches;
    /// [xyz] * 16 * numBezierPatches
    std::vector<float> _bezierVertices;

    // optional data
    /// [FarPatchParam] * numBezierPatches
    OpenSubdiv::Far::PatchParamTable _patchParams;
    /// [rgb] * numBezierPatches;
    std::vector<float> _colors;
    /// sharpness * numBezierPatches
    /// (for single-crease patch. needed until bezier conversion)
    std::vector<float> _sharpnesses;

    /// material index for each patches.
    std::vector<int> _materialIDs;
    std::vector<Material> _materials;
};

#endif  // OSD_RAYTRACE_MESH_H
