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
#include <ctime>
#include <cstring>
#include <string>
#include <cfloat>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

#include "common.h"
#include "scene.h"
#include "shader.h"
#include "context.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef OPENSUBDIV_HAS_TBB
#include <tbb/parallel_for.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/enumerable_thread_specific.h>
#endif

std::string
Scene::Config::Dump() const
{
    std::stringstream ss;
    ss << "Kernel = " << intersectKernel << ", "
       << "CropUV = " << cropUV << ", "
       << "BezierClip = " << bezierClip << ", "
       << "Eps level = " << epsLevel << ", "
       << "Max level = " << maxLevel << ", "
       << "Use Triangle = " << useTriangle << ", "
       << "Use RayDiffEpsilon = " << useRayDiffEpsilon;

    return ss.str();
}

// ---------------------------------------------------------------------------
Scene::Scene() : _vbo(0)
{
#ifdef OPENSUBDIV_HAS_TBB
    static tbb::task_scheduler_init init;
#endif
}

Scene::~Scene()
{
    if (_vbo) glDeleteBuffers(1, &_vbo);
}

void
Scene::BuildBVH(int minLeafPrimitives)
{
    BVHBuildOptions options; // Use default option

    options.minLeafPrimitives = minLeafPrimitives;

    printf("  BVH build option:\n");
    printf("    # of leaf primitives: %d\n", options.minLeafPrimitives);
    printf("    SAH binsize         : %d\n", options.binSize);

    printf("  # of triangles : %d\n", _mesh.GetNumTriangles());
    printf("  # of bezier patches : %d\n", _mesh.GetNumPatches());

    _accel = BVHAccel();
    _accel.Build(&_mesh, options);

    BVHBuildStatistics stats = _accel.GetStatistics();

    printf("  BVH statistics:\n");
    printf("    # of leaf   nodes: %d\n", stats.numLeafNodes);
    printf("    # of branch nodes: %d\n", stats.numBranchNodes);
    printf("  Max tree depth   : %d\n", stats.maxTreeDepth);
}

void
Scene::BuildVBO()
{
    // create vbo for display
    if (_vbo == 0) glGenBuffers(1, &_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, _accel.GetNodes().size() * sizeof(BVHNode),
                 &_accel.GetNodes()[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#ifdef OPENSUBDIV_HAS_TBB
typedef tbb::enumerable_thread_specific<Context> EtsContext;
EtsContext etsContexts;
#else
typedef std::vector<Context> EtsContext;
EtsContext etsContexts(1);
#endif

static double GetTraverseTime() {
    double time = 0;
    for (EtsContext::const_iterator it = etsContexts.begin();
         it != etsContexts.end(); ++it) {
        time += it->GetTraverseTime();
    }
    return time / etsContexts.size();
}
static double GetIntersectTime() {
    double time = 0;
    for (EtsContext::const_iterator it = etsContexts.begin();
         it != etsContexts.end(); ++it) {
        time += it->GetIntersectTime();
    }
    return time / etsContexts.size();
}
static double GetShadeTime() {
    double time = 0;
    for (EtsContext::const_iterator it = etsContexts.begin();
         it != etsContexts.end(); ++it) {
        time += it->GetShadeTime();
    }
    return time / etsContexts.size();
}
static void Reset() {
    for (EtsContext::iterator it = etsContexts.begin();
         it != etsContexts.end(); ++it) {
        it->Reset();
    }
}

class Kernel {
public:
    Kernel(int width, int stepIndex, int step, BVHAccel *accel,
           Camera *camera, float *image, Scene *scene, ShadeFunc shader) :
        _width(width), _stepIndex(stepIndex), _step(step), _accel(accel),
        _camera(camera), _image(image), _scene(scene), _shader(shader) {
        _subPixel[0] = rand()/(float)RAND_MAX;
        _subPixel[1] = rand()/(float)RAND_MAX;
    }

#ifdef OPENSUBDIV_HAS_TBB
    void operator() (tbb::blocked_range<int> const &r) const {
        bool useRayDiff = true;
        EtsContext::reference context = etsContexts.local();
        for (int rr = r.begin(); rr < r.end(); ++rr) {

#else
    void operator() (int begin, int end) const {
        bool useRayDiff = true;
        Context &context = etsContexts[0];
        for (int rr = begin; rr < end; ++rr) {
#endif
            int y = rr*_step + _stepIndex/_step;
            for (int x = _stepIndex%_step; x < _width; x += _step) {
                float offsetX = _subPixel[0];
                float offsetY = _subPixel[1];
                Ray ray = _camera->GenerateRay(x + 0.5f + offsetX,
                                               y + 0.5f + offsetY);
                if (useRayDiff) {
                    Ray rayO  = _camera->GenerateRay(x + offsetX,
                                                     y + offsetY);
                    Ray rayDX = _camera->GenerateRay(x + 1 + offsetX,
                                                     y + offsetY);
                    Ray rayDY = _camera->GenerateRay(x + offsetX,
                                                     y + 1 + offsetY);
                    ray.dDdx = rayDX.dir-rayO.dir;
                    ray.dDdy = rayDY.dir-rayO.dir;
                    ray.hasDifferential = true;
                }

                // intersection
                Intersection isect;
                bool hit = _accel->Traverse(ray, &isect, &context);

                // shading
                {
                    context.BeginShade();

                    float *d = _image + 4 * (y * _width + x);

                    // read pixel
                    float rgba[4];
                    rgba[0] = d[0];
                    rgba[1] = d[1];
                    rgba[2] = d[2];
                    rgba[3] = d[3];

                    // accumulate
                    if (hit) {
                        vec3f c = _shader(_scene, ray, isect);
                        rgba[0] += c[0];
                        rgba[1] += c[1];
                        rgba[2] += c[2];
                        rgba[3] += 1.0f;
                    } else {
                        vec3f bg = _scene->GetEnvColor(ray.dir);
                        rgba[0] += bg[0];///3.14;
                        rgba[1] += bg[1];///3.14;
                        rgba[2] += bg[2];///3.14;
                        rgba[3] += 1.0f;
                    }

                    // write back
                    d[0] = rgba[0];
                    d[1] = rgba[1];
                    d[2] = rgba[2];
                    d[3] = rgba[3];

                    context.EndShade();
                }
            }
        }
    }

private:
    int _width;
    int _stepIndex;
    int _step;
    BVHAccel *_accel;
    Camera *_camera;
    float *_image;
    Scene *_scene;
    float _subPixel[2];
    ShadeFunc _shader;
};

void
Scene::SetCamera(int width, int height, double fov,
                 std::vector<float> &image, // RGBA
                 const float eye[3], const float lookat[3], const float up[3])
{
    _width = width;
    _height = height;
    _image = &image[0];

    double deye[3] = { eye[0], eye[1], eye[2] };
    double dlook[3] = { lookat[0], lookat[1], lookat[2] };
    double dup[3] = { up[0], up[1], up[2] };

    _camera.BuildCameraFrame(deye, dlook, dup, fov, width, height);
    assert((int)image.size() >= 3 * width * height);
}

void
Scene::SetConfig(Config const &config)
{
    _accel.SetIntersectKernel(config.intersectKernel);
    _accel.SetUVMargin(config.uvMargin);
    _accel.SetCropUV(config.cropUV);
    _accel.SetBezierClip(config.bezierClip);

    static const double EPS_FROM_LEVEL[] = {
        1e-3,1e-3,1e-3,1e-3,1e-4,1e-5,1e-6,1e-7,1e-8,1e-9,
        1e-10,1e-11,1e-12,1e-13,1e-14,1e-15,1e-16,1e-17,1e-18,1e-19
    };

    _accel.SetEpsilon (EPS_FROM_LEVEL[config.epsLevel]);
    _accel.SetMaxLevel(config.maxLevel*2);
    _accel.SetUseTriangle(config.useTriangle);
    _accel.SetUseRayDiffEpsilon(config.useRayDiffEpsilon);
    _accel.SetConservativeTest(config.conservativeTest);
    _accel.SetDirectBilinear(config.directBilinear);
}

void
Scene::Render(int stepIndex, int step)
{
    // shader selection
    ShadeFunc shader;
    if (_mode == SHADED) {
        shader = ShadeLambert;
    } else if (_mode == PATCH_COORD) {
        shader = ShadePatchCoord;
    } else if (_mode == PATCH_TYPE) {
        shader = ShadePatchType;
    } else if (_mode == HEAT_MAP) {
        shader = ShadeHeatmap;
    } else if (_mode == PBS) {
        shader = ShadePBS;
    } else if (_mode == AO) {
        shader = ShadeAmbientOcclusion;
    } else {
        shader = ShadeLambert;
    }

#ifdef OPENSUBDIV_HAS_TBB
    tbb::blocked_range<int> range(0, _height/step, 1);

    Kernel kernel(_width, stepIndex, step, &_accel, &_camera, _image, this, shader);
    Reset();
    tbb::parallel_for(range, kernel);
#else
    Kernel kernel(_width, stepIndex, step, &_accel, &_camera, _image, this, shader);
    kernel(0, _height/step);
#endif

    _traverseTime += GetTraverseTime() * 1000;
    _intersectTime += GetIntersectTime() * 1000;
    _shadeTime += GetShadeTime() * 1000;
}

void
Scene::DebugTrace(float x, float y)
{
    printf("------------------------------------------\n");
    printf("Debug Trace at pixel %f, %f\n", x, y);

    float u = 0.5;
    float v = 0.5;

    Ray ray = _camera.GenerateRay(x + u, y + v);

    Intersection isect;
    bool hit = _accel.Traverse(ray, &isect, NULL);

    if (hit) {
        vec3f rgb = ShadePatchType(this, ray, isect);
        printf("Hit: %f %f %f\n", rgb[0], rgb[1], rgb[2]);
    } else {
        printf("Miss\n");
    }
}

void
Scene::RenderReport()
{
    int iteration = 10;

    _traverseTime = 0;
    _intersectTime = 0;
    _shadeTime = 0;

    Stopwatch s;
    s.Start();
    for (int i = 0; i < iteration; ++i) {
        Render();
    }
    s.Stop();
    float renderTime = s.GetElapsed();

    std::cout << "["
              << "Traverse = " << _traverseTime/1000.0/iteration << ", "
              << "Intersect = " << _intersectTime/1000.0/iteration << ", "
              << "Shade = " << _shadeTime/1000.0/iteration << ", "
              << "Total = " << renderTime/iteration << "], \n";
}

// envmap
bool
Scene::LoadEnvMap(const std::string &filename)
{
    int x, y, n;
    float *data = stbi_loadf(filename.c_str(), &x, &y, &n, 0);
    float gamma = 1.0f;

    Texture::Coordinate coord_type = Texture::COORDINATE_LONGLAT;

    if (data) {
        assert(n == 3); // RGB
        printf("envmap [%s] %d x %d\n", filename.c_str(), x, y);

        _envMap.Set(reinterpret_cast<const unsigned char *>(data), x, y, 3,
                    Texture::FORMAT_FLOAT, gamma, coord_type);
    } else {
        printf("Failed to load envmap [%s]\n", filename.c_str());
    }

    return true;
}

vec3f
Scene::GetEnvColor(const vec3f & dir) const
{
    float d[3];
    d[0] = dir[0];
    d[1] = dir[1];
    d[2] = dir[2];

    if (_backgroundMode == Scene::ENVMAP && _envMap.IsValid()) {
        float rgba[4];
        LongLatMapSampler::Sample(rgba, d, &_envMap);
        return vec3f(rgba[0], rgba[1], rgba[2])*3.14;
    } else {
        float l = pow((1.0+dir[2])*0.5, 100.0)*50.0f + 0.1f;
        return vec3f(l);
    }
}

