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
#ifndef OSD_RAYTRACE_SCENE_H
#define OSD_RAYTRACE_SCENE_H

#include "bvh_accel.h"
#include "camera.h"
#include "texture.h"
#include "mesh.h"
#include <far/patchTables.h>
#include <osd/opengl.h>
#include <string>

class Scene
{
public:
    struct Config {
        Config() {
            intersectKernel = BVHAccel::KERNEL_FLOAT;
            uvMargin = 0.0f;
            cropUV = false;
            bezierClip = true;
            epsLevel = 4;
            maxLevel = 16;
            useTriangle = false;
            useRayDiffEpsilon = true;
            conservativeTest = true;
            directBilinear = false;
            preTessLevel = 0;
            step = 1;
        }
        int intersectKernel;
        float uvMargin;
        bool cropUV;
        bool bezierClip;
        int epsLevel;
        int maxLevel;
        int step;
        bool useTriangle;
        bool useRayDiffEpsilon;
        bool conservativeTest;
        bool directBilinear;
        int preTessLevel;

        std::string Dump() const;
    };

    Scene();
    ~Scene();

    void BuildBVH(int minLeafPrimitives);
    void BuildVBO();

    void SetCamera(int width, int height, double fov,
                   std::vector<float> &image, // RGB
                   const float eye[3], const float lookat[3], const float up[3]);
    void SetConfig(Config const &config);

    // render
    void Render(int stepIndex, int step);
    void Render() { Render(0, 1); }
    bool Traverse(const Ray &ray,
                  Intersection *isect,
                  Context *context=NULL) const {
        return _accel.Traverse(ray, isect, context);
    }

    // debug
    void DebugTrace(float x, float y);

    // shading style
    enum ShadeMode { SHADED, PATCH_COORD, PATCH_TYPE, HEAT_MAP, AO, PBS };
    void SetShadeMode(ShadeMode mode) {
        _mode = mode;
    }

    // background style
    enum BackgroundMode { GRADATION, WHITE, BLACK, ENVMAP };
    void SetBackgroudMode(BackgroundMode mode) {
        _backgroundMode = mode;
    }
    BackgroundMode GetBackgroundMode() const {
        return _backgroundMode;
    }

    // envmap
    bool LoadEnvMap(const std::string &filename);
    vec3f GetEnvColor(const vec3f &dir) const;

    // mesh
    Mesh &GetMesh() { return _mesh; }
    const Mesh &GetMesh() const { return _mesh; }

    // BVH visualize
    GLuint GetVBO() const { return _vbo; }
    int GetNumBVHNode() const { return (int)_accel.GetNodes().size(); }

    // reporting
    void RenderReport();
    size_t GetMemoryUsage() const {
        size_t mem = 0;
        // bvh
        mem += _accel.GetNodes().size() * sizeof(BVHNode);
        mem += _accel.GetIndices().size() * sizeof(unsigned int);
        return mem;
    }

private:
    Camera _camera;
    Mesh _mesh;
    BVHAccel _accel;
    ShadeMode _mode;
    BackgroundMode _backgroundMode;

    Texture _envMap;

    double _traverseTime;
    double _intersectTime;
    double _shadeTime;

    GLuint _vbo;
    int _width;
    int _height;
    float *_image;
};

#endif  // OSD_RAYTRACE_SCENE_H
