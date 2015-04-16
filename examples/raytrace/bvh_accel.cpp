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
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <memory>
#include <limits>

#include "bvh_accel.h"
#include "mesh.h"

#include "bezier/bezier.h"
#include "bezier/bezierIntersect.h"
#include "bezier/math.h"
#include "bezier/math_sse.h"

#if ENABLE_TRACE_PRINT
#define trace(...) { printf(__VA_ARGS__); }
#else
#define trace(...)
#endif

#if ENABLE_DEBUG_PRINT
#define debug(f, ...) { printf(f, __VA_ARGS__); }
#else
#define debug(f, ...)
#endif

using namespace OsdBezier;

//
//   The original BVH code in this file was contributed by Syoyo Fujita and
//   Toru Matsuoka.
//

//
// SAH functions
//

struct BinBuffer {
    BinBuffer(int size) {
        binSize = size;
        bin.resize(2 * 3 * size);
        clear();
    }
    void clear() { memset(&bin[0], 0, sizeof(size_t) * 2 * 3 * binSize); }

    std::vector<size_t> bin; // (min, max) * xyz * binsize
    int binSize;
};

static inline float
CalculateSurfaceArea(const vec3f &min, const vec3f &max)
{
    vec3f box = max - min;
    return 2.0f * (box[0] * box[1] + box[1] * box[2] + box[2] * box[0]);
}

static inline void
GetBoundingBoxOfTriangle(vec3f &bmin, vec3f &bmax,
                         const Mesh *mesh, unsigned int index)
{
    unsigned int f0 = mesh->GetFaces()[3 * index + 0];
    unsigned int f1 = mesh->GetFaces()[3 * index + 1];
    unsigned int f2 = mesh->GetFaces()[3 * index + 2];

    vec3f p[3];

    p[0] = vec3f(&mesh->GetTriangleVerts()[3 * f0]);
    p[1] = vec3f(&mesh->GetTriangleVerts()[3 * f1]);
    p[2] = vec3f(&mesh->GetTriangleVerts()[3 * f2]);

    bmin = p[0];
    bmax = p[0];

    for (int i = 1; i < 3; i++) {
        bmin = bmin.min(p[i]);
        bmax = bmax.max(p[i]);
    }
}

static inline void
GetBoundingBoxOfRegularPatch(vec3f &bmin, vec3f &bmax,
                             const Mesh *mesh, unsigned int index)
{
    BezierPatch<vec3f, float, 4> patch(mesh->GetBezierVerts(index));
    patch.GetMinMax(bmin, bmax);
}

static void
ContributeBinBuffer(BinBuffer *bins, // [out]
                    const vec3f &sceneMin, const vec3f &sceneMax,
                    const Mesh *mesh, unsigned int *indices,
                    unsigned int leftIdx, unsigned int rightIdx)
{
    static const float EPS = std::numeric_limits<float>::epsilon() * 1024;

    float binSize = (float)bins->binSize;

    // Calculate extent
    vec3f sceneSize, sceneInvSize;
    sceneSize = sceneMax - sceneMin;
    for (int i = 0; i < 3; ++i) {
        assert(sceneSize[i] >= 0.0);

        if (sceneSize[i] > EPS) {
            sceneInvSize[i] = binSize / sceneSize[i];
        } else {
            sceneInvSize[i] = 0.0;
        }
    }

    // Clear bin data
    memset(&bins->bin[0], 0, sizeof(2 * 3 * bins->binSize));

    size_t idxBMin[3];
    size_t idxBMax[3];

    bool bezierMesh = mesh->IsBezierMesh();

    for (size_t i = leftIdx; i < rightIdx; i++) {
        //
        // Quantize the position into [0, BIN_SIZE)
        //
        // q[i] = (int)(p[i] - scene_bmin) / scene_size
        //
        vec3f bmin;
        vec3f bmax;

        if (bezierMesh) {
            GetBoundingBoxOfRegularPatch(bmin, bmax, mesh, indices[i]);
        } else {
            GetBoundingBoxOfTriangle(bmin, bmax, mesh, indices[i]);
        }

        vec3f quantizedBMin = (bmin - sceneMin) * sceneInvSize;
        vec3f quantizedBMax = (bmax - sceneMin) * sceneInvSize;

        // idx is now in [0, BIN_SIZE)
        for (size_t j = 0; j < 3; ++j) {
            idxBMin[j] = (unsigned int)floor(quantizedBMin[j]);
            idxBMax[j] = (unsigned int)floor(quantizedBMax[j]);

            if (idxBMin[j] >= binSize)
                idxBMin[j] = binSize - 1;
            if (idxBMax[j] >= binSize)
                idxBMax[j] = binSize - 1;

            assert(idxBMin[j] < binSize);
            assert(idxBMax[j] < binSize);

            // Increment bin counter
            bins->bin[0*(bins->binSize*3) + j*bins->binSize + idxBMin[j]] += 1;
            bins->bin[1*(bins->binSize*3) + j*bins->binSize + idxBMax[j]] += 1;
        }
    }
}

static inline double
SAH(size_t ns1, float leftArea, size_t ns2, float rightArea,
    float invS, float Taabb, float Ttri)
{
    // const float Taabb = 0.2f;
    // const float Ttri = 0.8f;
    return 2.0f * Taabb
        + (leftArea * invS) * (float)(ns1)*Ttri
        + (rightArea * invS) * (float)(ns2)*Ttri;
}

static void
FindCutFromBinBuffer(float *cutPos,     // [out] xyz
                     int &minCostAxis, // [out]
                     const BinBuffer *bins, const vec3f &bmin,
                     const vec3f &bmax, size_t numTriangles,
                     float costTaabb) // should be in [0.0, 1.0]
{
    const float eps = std::numeric_limits<float>::epsilon() * 1024;

    size_t left, right;
    vec3f bsize, bstep;
    vec3f bminLeft, bmaxLeft;
    vec3f bminRight, bmaxRight;
    float saLeft, saRight, saTotal;
    float pos;
    float minCost[3];
    float costTtri = 1.0 - costTaabb;

    minCostAxis = 0;
    bsize = bmax - bmin;
    bstep = bsize * (1.0 / bins->binSize);
    saTotal = CalculateSurfaceArea(bmin, bmax);

    float invSaTotal = 0.0;
    if (saTotal > eps) {
        invSaTotal = 1.0 / saTotal;
    }

    for (int j = 0; j < 3; ++j) {
        //
        // Compute SAH cost for right side of each cell of the bbox.
        // Exclude both extreme side of the bbox.
        //
        //  i:      0    1    2    3
        //     +----+----+----+----+----+
        //     |    |    |    |    |    |
        //     +----+----+----+----+----+
        //

        float minCostPos = bmin[j] + 0.5 * bstep[j];
        minCost[j] = std::numeric_limits<float>::max();

        left = 0;
        right = numTriangles;
        bminLeft = bminRight = bmin;
        bmaxLeft = bmaxRight = bmax;

        for (int i = 0; i < bins->binSize - 1; ++i) {
            left += bins->bin[0 * (3 * bins->binSize) + j * bins->binSize + i];
            right -= bins->bin[1 * (3 * bins->binSize) + j * bins->binSize + i];

            assert(left <= numTriangles);
            assert(right <= numTriangles);

            //
            // Split pos bmin + (i + 1) * (bsize / BIN_SIZE)
            // +1 for i since we want a position on right side of the cell.
            //

            pos = bmin[j] + (i + 0.5) * bstep[j];
            bmaxLeft[j] = pos;
            bminRight[j] = pos;

            saLeft = CalculateSurfaceArea(bminLeft, bmaxLeft);
            saRight = CalculateSurfaceArea(bminRight, bmaxRight);

            float cost =
                SAH(left, saLeft, right, saRight, invSaTotal, costTaabb, costTtri);
            if (cost < minCost[j]) {
                //
                // Update the min cost
                //
                minCost[j] = cost;
                minCostPos = pos;
                // minCostAxis = j;
            }
        }
        cutPos[j] = minCostPos;
    }

    // cutAxis = minCostAxis;
    // cutPos = minCostPos;
    // Find min cost axis
    float cost = minCost[0];
    minCostAxis = 0;
    if (cost > minCost[1]) {
        minCostAxis = 1;
        cost = minCost[1];
    }
    if (cost > minCost[2]) {
        minCostAxis = 2;
        cost = minCost[2];
    }
}

class SAHPred : public std::unary_function<unsigned int, bool> {
public:
    SAHPred(int axis, float pos, const Mesh *mesh)
        : axis_(axis), pos_(pos), mesh_(mesh) {
        bezier_ = mesh->IsBezierMesh();
    }

    bool operator()(unsigned int i) const {
        int axis = axis_;
        float pos = pos_;

        if (bezier_) {
            float center = 0;
            for (int j = 0; j < 16; ++j) {
                vec3f v = *(mesh_->GetBezierVerts(i) + j);
                center += v[axis];
            }
            return (center < pos * 16.0);
        } else {
            unsigned int i0 = mesh_->GetFaces()[3 * i + 0];
            unsigned int i1 = mesh_->GetFaces()[3 * i + 1];
            unsigned int i2 = mesh_->GetFaces()[3 * i + 2];
            vec3f p0(&mesh_->GetTriangleVerts()[3 * i0]);
            vec3f p1(&mesh_->GetTriangleVerts()[3 * i1]);
            vec3f p2(&mesh_->GetTriangleVerts()[3 * i2]);

            float center = p0[axis] + p1[axis] + p2[axis];

            return (center < pos * 3.0);
        }
    }

private:
    int axis_;
    float pos_;
    const Mesh *mesh_;
    bool bezier_;
};

static void ComputeBoundingBox(vec3f &bmin, vec3f &bmax,
                               const vec3f *bezierVertices,
                               const unsigned int *indices,
                               unsigned int leftIndex,
                               unsigned int rightIndex)
{
    const float kEPS = std::numeric_limits<float>::epsilon() * 1024;

    size_t i = leftIndex;
    size_t idx = indices[i];

    BezierPatch<vec3f, float, 4> patch(&bezierVertices[idx * 16]);
    patch.GetMinMax(bmin, bmax);

    for (i = leftIndex; i < rightIndex; i++) { // for each faces
        size_t idx = indices[i];
        vec3f pmin, pmax;
        BezierPatch<vec3f, float, 4> patch(&bezierVertices[idx * 16]);
        patch.GetMinMax(pmin, pmax);
        bmin = bmin.min(pmin);
        bmax = bmax.max(pmax);
    }
    bmin[0] -= kEPS;
    bmin[1] -= kEPS;
    bmin[2] -= kEPS;
    bmax[0] += kEPS;
    bmax[1] += kEPS;
    bmax[2] += kEPS;
}

static void ComputeBoundingBox(vec3f &bmin, vec3f &bmax,
                               const float *vertices,
                               const unsigned int *faces,
                               const unsigned int *indices,
                               unsigned int leftIndex,
                               unsigned int rightIndex) {
    const float kEPS = std::numeric_limits<float>::epsilon() * 1024;

    size_t i = leftIndex;
    size_t idx = indices[i];

    bmin[0] = vertices[3 * faces[3 * idx + 0] + 0] - kEPS;
    bmin[1] = vertices[3 * faces[3 * idx + 0] + 1] - kEPS;
    bmin[2] = vertices[3 * faces[3 * idx + 0] + 2] - kEPS;
    bmax[0] = vertices[3 * faces[3 * idx + 0] + 0] + kEPS;
    bmax[1] = vertices[3 * faces[3 * idx + 0] + 1] + kEPS;
    bmax[2] = vertices[3 * faces[3 * idx + 0] + 2] + kEPS;

    // Assume mesh are composed of all triangles
    for (i = leftIndex; i < rightIndex; i++) { // for each faces
        size_t idx = indices[i];
        for (int j = 0; j < 3; j++) { // for each face vertex
            size_t fid = faces[3 * idx + j];
            for (int k = 0; k < 3; k++) { // xyz
                float minval = vertices[3 * fid + k] - kEPS;
                float maxval = vertices[3 * fid + k] + kEPS;
                if (bmin[k] > minval)
                    bmin[k] = minval;
                if (bmax[k] < maxval)
                    bmax[k] = maxval;
            }
        }
    }
}

//
// --
//

size_t
BVHAccel::BuildTree(unsigned int leftIdx, unsigned int rightIdx, int depth)
{
    assert(leftIdx <= rightIdx);

    debug("d: %d, l: %d, r: %d\n", depth, leftIdx, rightIdx);

    size_t offset = _nodes.size();

    if (_stats.maxTreeDepth < depth) {
        _stats.maxTreeDepth = depth;
    }

    vec3f bmin, bmax;
    if (_mesh->IsBezierMesh()) {
        ComputeBoundingBox(bmin, bmax, _mesh->GetBezierVerts(0),
                           &_indices.at(0), leftIdx, rightIdx);
    } else {
        ComputeBoundingBox(bmin, bmax, _mesh->GetTriangleVerts(),
                           _mesh->GetFaces(),
                           &_indices.at(0), leftIdx, rightIdx);
    }

    debug(" bmin = %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
    debug(" bmax = %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);

    size_t n = rightIdx - leftIdx;
    if ((n < (size_t)_options.minLeafPrimitives) ||
        (depth >= _options.maxTreeDepth)) {
        // Create leaf node.
        BVHNode leaf;
        leaf.bmin = bmin;
        leaf.bmax = bmax;

        assert(leftIdx < std::numeric_limits<unsigned int>::max());

        leaf.flag = 1; // leaf
        leaf.data[0] = n;
        leaf.data[1] = (unsigned int)leftIdx;
        debug(" leaf n = %d, offt = %d\n", n, leftIdx);

        _nodes.push_back(leaf);
        _stats.numLeafNodes++;

        return offset;
    }

    //
    // Create branch node.
    //

    //
    // Compute SAH and find best split axis and position
    //
    int minCutAxis = 0;
    float cutPos[3] = {0.0, 0.0, 0.0};

    BinBuffer bins(_options.binSize);
    ContributeBinBuffer(&bins, bmin, bmax, _mesh, &_indices.at(0), leftIdx,
                        rightIdx);
    FindCutFromBinBuffer(cutPos, minCutAxis, &bins, bmin, bmax, n,
                         _options.costTaabb);

    debug("depth: %d, cutPos: (%f, %f, %f), cutAxis: %d, l = %d, r = %d\n",
          depth, cutPos[0],
          cutPos[1], cutPos[2], minCutAxis, leftIdx, rightIdx);

    // Try all 3 axis until good cut position avaiable.
    unsigned int midIdx;
    int cutAxis = minCutAxis;
    for (int axisTry = 0; axisTry < 3; axisTry++) {

        unsigned int *begin = &_indices[leftIdx];
        unsigned int endIdx = std::max(leftIdx, rightIdx - 1);
        unsigned int *end = &_indices[endIdx];
        unsigned int *mid = 0;

        // try minCutAxis first.
        cutAxis = (minCutAxis + axisTry) % 3;

        //
        // Split at (cutAxis, cutPos)
        // _indices will be modified.
        //
        mid = std::partition(begin, end, SAHPred(cutAxis, cutPos[cutAxis], _mesh));

        midIdx = leftIdx + (mid - begin);
        if ((midIdx == leftIdx) || (midIdx == rightIdx)) {

            // Can't split well.
            // Switch to object median(which may create unoptimized tree, but stable)
            midIdx = leftIdx + (n >> 1);

            // Try another axis if there's axis to try.
        } else {
            // Found good cut. exit loop.
            break;
        }
    }
    BVHNode node;
    node.axis = cutAxis;
    node.flag = 0; // 0 = branch
    _nodes.push_back(node);

    // Recurively split tree.
    unsigned int leftChildIndex = BuildTree(leftIdx, midIdx, depth + 1);
    unsigned int rightChildIndex = BuildTree(midIdx, rightIdx, depth + 1);

    _nodes[offset].data[0] = leftChildIndex;
    _nodes[offset].data[1] = rightChildIndex;

    _nodes[offset].bmin = bmin;
    _nodes[offset].bmax = bmax;

    _stats.numBranchNodes++;

    return offset;
}

bool
BVHAccel::Build(const Mesh *mesh, const BVHBuildOptions &options)
{
    if (mesh == NULL) return false;

    _mesh = mesh;
    _options = options;
    _stats = BVHBuildStatistics();

    assert(_options.binSize > 1);

    size_t n = mesh->GetNumPrimitives();
    trace("[BVHAccel] Input # of bezier patches = %lu\n", mesh->GetNumPatches());

    //
    // 1. Create triangle indices(this will be permutated in BuildTree)
    //
    _indices.resize(n);
    for (size_t i = 0; i < n; i++) {
        _indices[i] = i;
    }

    //
    // 2. Build tree
    //
    BuildTree(0, n, 0);

    // Tree will be null if input triangle count == 0.
    if (!_nodes.empty()) {
      // 0 = root node.
        vec3f bmin(&_nodes[0].bmin[0]);
        vec3f bmax(&_nodes[0].bmax[0]);
        trace("[BVHAccel] bound min = (%f, %f, %f)\n", bmin[0], bmin[1], bmin[2]);
        trace("[BVHAccel] bound max = (%f, %f, %f)\n", bmax[0], bmax[1], bmax[2]);
    }

    trace("[BVHAccel] # of nodes = %lu\n", _nodes.size());
    return true;
}

static bool
IntersectRayAABB(float &tminOut, // [out]
                 float &tmaxOut, // [out]
                 float maxT, vec3f bmin, vec3f bmax,
                 Ray const &ray)
{
    float tmin, tmax;

    const float min_x = ray.dirSign[0] * bmax[0] + (1-ray.dirSign[0]) * bmin[0];
    const float min_y = ray.dirSign[1] * bmax[1] + (1-ray.dirSign[1]) * bmin[1];
    const float min_z = ray.dirSign[2] * bmax[2] + (1-ray.dirSign[2]) * bmin[2];
    const float max_x = ray.dirSign[0] * bmin[0] + (1-ray.dirSign[0]) * bmax[0];
    const float max_y = ray.dirSign[1] * bmin[1] + (1-ray.dirSign[1]) * bmax[1];
    const float max_z = ray.dirSign[2] * bmin[2] + (1-ray.dirSign[2]) * bmax[2];

    // X
    const double tmin_x = (min_x - ray.org[0]) * ray.invDir[0];
    const double tmax_x = (max_x - ray.org[0]) * ray.invDir[0];

    // Y
    const double tmin_y = (min_y - ray.org[1]) * ray.invDir[1];
    const double tmax_y = (max_y - ray.org[1]) * ray.invDir[1];

    tmin = (tmin_x > tmin_y) ? tmin_x : tmin_y;
    tmax = (tmax_x < tmax_y) ? tmax_x : tmax_y;

    // Z
    const double tmin_z = (min_z - ray.org[2]) * ray.invDir[2];
    const double tmax_z = (max_z - ray.org[2]) * ray.invDir[2];

    tmin = (tmin > tmin_z) ? tmin : tmin_z;
    tmax = (tmax < tmax_z) ? tmax : tmax_z;

    //
    // Hit include (tmin == tmax) edge case(hit 2D plane).
    //
#if 1
    if ((tmax > 0.0) && (tmin <= tmax) && (tmin <= maxT)) {

        tminOut = tmin;
        tmaxOut = tmax;

        return true;
    }

    return false; // no hit
#else
    return (tmax > 0.0) & (tmin <= tmax) & (tmin <= maxT);
#endif
}

static inline bool
TriangleIsect(float &tInOut, float &uOut, float &vOut, const vec3f &v0,
              const vec3f &v1, const vec3f &v2, const vec3f &rayOrg,
              const vec3f &rayDir)
{
    const float kEPS = std::numeric_limits<float>::epsilon() * 1024;

    vec3f p0(v0[0], v0[1], v0[2]);
    vec3f p1(v1[0], v1[1], v1[2]);
    vec3f p2(v2[0], v2[1], v2[2]);
    vec3f e1, e2;
    vec3f p, s, q;

    e1 = p1 - p0;
    e2 = p2 - p0;
    p = cross(rayDir, e2);

    float invDet;
    float det = dot(e1, p);
    if (std::abs(det) < kEPS) { // no-cull
        //    return false;
    }

    invDet = 1.0 / det;

    s = rayOrg - p0;
    q = cross(s, e1);

    float u = dot(s, p) * invDet;
    float v = dot(q, rayDir) * invDet;
    float t = dot(e2, q) * invDet;

    if (u < 0.0 || u > 1.0)
        return false;
    if (v < 0.0 || u + v > 1.0)
        return false;
    if (t < 0.0 || t > tInOut)
        return false;

    tInOut = t;
    uOut = u;
    vOut = v;

    return true;
}

template<typename T>
bool PatchIsect(int patchIndex,
                Intersection *isect,
                const vec3f *bezierVerts,
                int wcpFlag,
                const Ray &ray,
                float uvMargin,
                bool cropUV,
                bool bezierClip,
                double eps,
                int maxLevel,
                bool useTriangle,
                bool useRayDiffEpsilon,
                bool directBilinear)
{
    BezierPatchIntersection<T, typename T::ElementType, 4> bzi(bezierVerts);
    if (useRayDiffEpsilon) {
        eps = bzi.ComputeEpsilon(ray, eps);
    }

    bzi.SetEpsilon(eps);
    bzi.SetMaxLevel(maxLevel);
    bzi.SetCropUV  (cropUV);
    bzi.SetUVMergin(uvMargin);
    bzi.SetUseBezierClip(bezierClip);
    bzi.SetUseTriangle  (useTriangle);
    bzi.SetDirectBilinear(directBilinear);
    bzi.SetWatertightFlag(wcpFlag);

    float t = isect->t;
    return bzi.Test(isect, ray, 0, t);
}

bool
BVHAccel::TestLeafNode(Intersection *isect, // [inout]
                       const BVHNode &node,
                       const Ray &ray) const
{
    bool hit = false;

    unsigned int numPrimitives = node.data[0];
    unsigned int offset = node.data[1];

    float t = isect->t;

    vec3f rayOrg = ray.org;
    vec3f rayDir = ray.dir;

    Ray tr = ray;

    if (_mesh->IsBezierMesh()) {
        for (unsigned int i = 0; i < numPrimitives; i++) {
            int faceIdx = _indices[i + offset];

            const vec3f *bv = _mesh->GetBezierVerts(faceIdx);
            int wcpFlag = _conservativeTest ? _mesh->GetWatertightFlag(faceIdx) : 0;

            bool r = false;
            if (_intersectKernel == BVHAccel::KERNEL_FLOAT) {
                r = PatchIsect<vec3f>(faceIdx, isect, bv, wcpFlag, tr,
                                      _uvMargin, _cropUV, _bezierClip,
                                      _epsilon,
                                      _maxLevel, _useTriangle,
                                      _useRayDiffEpsilon, _directBilinear);
            } else if (_intersectKernel == BVHAccel::KERNEL_SSE) {
                r = PatchIsect<vec3sse>(faceIdx, isect, bv, wcpFlag, tr,
                                        _uvMargin, _cropUV, _bezierClip,
                                        _epsilon,
                                        _maxLevel, _useTriangle,
                                        _useRayDiffEpsilon, _directBilinear);
            } else if (_intersectKernel == BVHAccel::KERNEL_DOUBLE) {
                r = PatchIsect<vec3d>(faceIdx, isect, bv, wcpFlag, tr,
                                      _uvMargin, _cropUV, _bezierClip,
                                      _epsilon,
                                      _maxLevel, _useTriangle,
                                      _useRayDiffEpsilon, _directBilinear);
            }
            if (r) {
                // Update isect state
                isect->faceID = faceIdx;
                isect->matID = 0;
                hit = true;
            }
        }
    } else {
        for (unsigned int i = 0; i < numPrimitives; i++) {
            int faceIdx = _indices[i + offset];

            int f0 = _mesh->GetFaces()[3 * faceIdx + 0];
            int f1 = _mesh->GetFaces()[3 * faceIdx + 1];
            int f2 = _mesh->GetFaces()[3 * faceIdx + 2];

            vec3f v0, v1, v2;
            v0[0] = _mesh->GetTriangleVerts()[3 * f0 + 0];
            v0[1] = _mesh->GetTriangleVerts()[3 * f0 + 1];
            v0[2] = _mesh->GetTriangleVerts()[3 * f0 + 2];

            v1[0] = _mesh->GetTriangleVerts()[3 * f1 + 0];
            v1[1] = _mesh->GetTriangleVerts()[3 * f1 + 1];
            v1[2] = _mesh->GetTriangleVerts()[3 * f1 + 2];

            v2[0] = _mesh->GetTriangleVerts()[3 * f2 + 0];
            v2[1] = _mesh->GetTriangleVerts()[3 * f2 + 1];
            v2[2] = _mesh->GetTriangleVerts()[3 * f2 + 2];

            float u, v;
            if (TriangleIsect(t, u, v, v0, v1, v2, rayOrg, rayDir)) {
                // Update isect state
                isect->t = t;
                isect->u = u;
                isect->v = v;
                isect->faceID = faceIdx;
                isect->matID = 0;
                hit = true;
            }
        }
    }
    return hit;
}

static void
BuildIntersection(Intersection *isect, const Mesh *mesh, const Ray &ray)
{
    if (mesh->IsBezierMesh()) {
        //const OpenSubdiv::Far::PatchParam &param = mesh->_patchParams[isect->faceID];
        const OpenSubdiv::Far::PatchParam &param = mesh->GetPatchParam(isect->faceID);
        int matID = mesh->GetMaterialID(isect->faceID);

        unsigned int bits = param.bitField.field;
        isect->patchID = isect->faceID;
        isect->faceID = param.faceIndex; // back to ptex face index
        isect->matID = matID;
        isect->level = (bits & 0xf);
        isect->position = ray.org + isect->t * ray.dir;
#if 0
        // remap ptex index (if neccessary)
        int level = 1 << ((bits & 0xf) - ((bits >> 4) &1));
        int pu = (bits >> 17) & 0x3ff;
        int pv = (bits >> 7) & 0x3ff;
        int rot = (bits >> 5) & 0x3;

        float u = float(rot==0)*(isect->v)
            + float(rot==1)*(1-isect->u)
            + float(rot==2)*(1-isect->v)
            + float(rot==3)*(isect->u);
        float v = float(rot==0)*(isect->u)
            + float(rot==1)*(isect->v)
            + float(rot==2)*(1-isect->u)
            + float(rot==3)*(1-isect->v);

        isect->u = (u + pu)/(float)level;
        isect->v = (v + pv)/(float)level;
#endif
    } else {
        // face index
        isect->patchID = isect->faceID;
        isect->level = 1;

        const unsigned int *faces = mesh->GetFaces();
        isect->f0 = faces[3 * isect->faceID + 0];
        isect->f1 = faces[3 * isect->faceID + 1];
        isect->f2 = faces[3 * isect->faceID + 2];

        const float *vertices = mesh->GetTriangleVerts();
        vec3f p0(&vertices[3 * isect->f0]);
        vec3f p1(&vertices[3 * isect->f1]);
        vec3f p2(&vertices[3 * isect->f2]);

        // calc shading point.
        isect->position = ray.org + isect->t * ray.dir;

        // interpolate normal
        const float *normals = mesh->GetTriangleNormals();
        vec3f n0(&normals[3 * isect->f0]);
        vec3f n1(&normals[3 * isect->f1]);
        vec3f n2(&normals[3 * isect->f2]);

        vec3f n = n1 * isect->u + n2 * isect->v + n0 * (1 - isect->u - isect->v);
        n.normalize();
        n = n.neg();

        isect->geometricNormal = n;
        isect->normal = n;
    }
}

#define kMaxStackDepth    512

static inline float
inverse(float x)
{
    if (fabs(x) < 1e-16) return 1e+16;
    return float(1)/x;
}

bool
BVHAccel::Traverse(Ray ray, Intersection *isect, Context *context) const
{
    if (context) context->BeginTraverse();

    float hitT = std::numeric_limits<float>::max(); // far = no hit.

    int nodeStackIndex = 0;
    int nodeStack[kMaxStackDepth+1];
    nodeStack[0] = 0;

    // Init isect info as no hit
    isect->t = hitT;
    isect->u = 0.0;
    isect->v = 0.0;
    isect->faceID = -1;
    isect->matID = 0;
    isect->maxLevel = _maxLevel;

    ray.invDir = vec3f(inverse(ray.dir[0]),
                       inverse(ray.dir[1]),
                       inverse(ray.dir[2]));
    ray.dirSign[0] = ray.dir[0] < 0.0 ? 1 : 0;
    ray.dirSign[1] = ray.dir[1] < 0.0 ? 1 : 0;
    ray.dirSign[2] = ray.dir[2] < 0.0 ? 1 : 0;

    float minT, maxT;
    while (nodeStackIndex >= 0) {
        int index = nodeStack[nodeStackIndex];
        const BVHNode &node = _nodes[index];

        nodeStackIndex--;

        bool hit = IntersectRayAABB(minT, maxT, hitT, node.bmin, node.bmax, ray);

        if (node.flag == 0) { // branch node
            if (hit) {
                int orderNear = ray.dirSign[node.axis];
                int orderFar = 1 - orderNear;

                // Traverse near first.
                nodeStack[++nodeStackIndex] = node.data[orderFar];
                nodeStack[++nodeStackIndex] = node.data[orderNear];
            }
        } else { // leaf node
            if (hit) {
                if (context) {
                    context->EndTraverse();
                    context->BeginIntersect();
                }

                if (TestLeafNode(isect, node, ray)) {
                    hitT = isect->t;
                }

                if (context) {
                    context->EndIntersect();
                    context->BeginTraverse();
                }
            }
        }
        if (nodeStackIndex >= kMaxStackDepth) {
            trace("node stack overflow\n");
            return false;
        }
    }

    if (isect->t < std::numeric_limits<float>::max()) {
        BuildIntersection(isect, _mesh, ray);

        if (context) context->EndTraverse();
        return true;
    }

    if (context) context->EndTraverse();
    return false;
}
