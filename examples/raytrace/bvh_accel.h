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
#ifndef __BVH_ACCEL_H__
#define __BVH_ACCEL_H__

#include <vector>
#include "ray.h"
#include "context.h"
#include "common.h"

class Mesh;

class BVHNode {
public:
    BVHNode() {};
    ~BVHNode() {};

    vec3f bmin, bmax;
    int flag; // 1 = leaf node, 0 = branch node
    int axis;
    // leaf
    //   data[0] = npoints
    //   data[1] = index
    //
    // branch
    //   data[0] = child[0]
    //   data[1] = child[1]
    unsigned int data[2];
};

///< BVH build option.
struct BVHBuildOptions {
    bool debugPrint;
    float costTaabb;
    int minLeafPrimitives;
    int maxTreeDepth;
    int binSize;

    // Set default value: Taabb = 0.2
    BVHBuildOptions()
        : costTaabb(0.2), minLeafPrimitives(16),
          maxTreeDepth(256), binSize(64) {}
};

///< BVH build statistics.
struct BVHBuildStatistics {
    int maxTreeDepth;
    int numLeafNodes;
    int numBranchNodes;

    // Set default value: Taabb = 0.2
    BVHBuildStatistics() : maxTreeDepth(0), numLeafNodes(0), numBranchNodes(0) {}
};

class BVHAccel {
public:
    BVHAccel(float uvMargin=0) :
        _intersectKernel(KERNEL_FLOAT),
        _epsilon(1e-4),
        _uvMargin(uvMargin), _cropUV(true), _bezierClip(true),
        _maxLevel(10), _useTriangle(false),
        _useRayDiffEpsilon(false), _conservativeTest(false),
        _directBilinear(false) {}
    ~BVHAccel() {};

    /// kernel type
    enum { KERNEL_FLOAT, KERNEL_SSE, KERNEL_DOUBLE } IntersectKernel;

    ///< Build BVH for input mesh.
    bool Build(const Mesh *mesh, const BVHBuildOptions &options);

    ///< Get statistics of built BVH tree. Valid after Build()
    BVHBuildStatistics GetStatistics() const { return _stats; }

    ///< Traverse into BVH along ray and find closest hit point if found
    bool Traverse(Ray ray, Intersection *isect, Context *context) const;

    void SetIntersectKernel(int k) {_intersectKernel = k; }
    void SetUVMargin(float margin) { _uvMargin = margin; }
    void SetCropUV(bool flag) {_cropUV = flag;}
    void SetBezierClip(bool flag) {_bezierClip = flag;}

    void SetEpsilon(double eps){_epsilon=eps;}
    void SetMaxLevel(int level){_maxLevel=level;}
    void SetUseTriangle(bool flag){_useTriangle=flag;}
    void SetUseRayDiffEpsilon(bool flag){_useRayDiffEpsilon=flag;}
    void SetConservativeTest(bool flag){_conservativeTest=flag;}
    void SetDirectBilinear(bool flag){_directBilinear=flag;}

    const std::vector<BVHNode> &GetNodes() const { return _nodes; }
    const std::vector<unsigned int> &GetIndices() const { return _indices; }

private:
    ///< Builds BVH tree recursively.
    size_t BuildTree(unsigned int leftIdx, unsigned int rightIdx, int depth);

    bool TestLeafNode(Intersection *isect, // [inout]
                      const BVHNode &node,
                      const Ray &ray) const;

    BVHBuildOptions _options;
    std::vector<BVHNode> _nodes;
    std::vector<unsigned int> _indices; // max 4G triangles.
    BVHBuildStatistics _stats;

    const Mesh *_mesh;
    int _intersectKernel;
    double _epsilon;

    float _uvMargin;
    bool _cropUV;
    bool _bezierClip;
    int  _maxLevel; 
    bool _useTriangle;
    bool _useRayDiffEpsilon;
    bool _conservativeTest;
    bool _directBilinear;
};

#endif // __BVH_ACCEL_H__
       // vim:set sw=2 ts=2 expandtab:
