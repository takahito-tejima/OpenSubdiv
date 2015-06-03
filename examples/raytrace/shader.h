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
#ifndef OSD_RAYTRACE_SHADER_H
#define OSD_RAYTRACE_SHADER_H

#include "common.h"

struct Intersection;
struct Ray;
class Scene;

typedef vec3f (*ShadeFunc)(const Scene *, const Ray &, const Intersection &);

extern vec3f ShadeLambert(const Scene *scene,
                          const Ray &ray,
                          const Intersection &isect);
extern vec3f ShadePatchCoord(const Scene *scene,
                             const Ray &ray,
                             const Intersection &isect);
extern vec3f ShadeHeatmap(const Scene *scene,
                          const Ray &ray,
                          const Intersection &isect);
extern vec3f ShadePatchType(const Scene *scene,
                            const Ray &ray,
                            const Intersection &isect);
extern vec3f ShadeAmbientOcclusion(const Scene *scene,
                                   const Ray &ray,
                                   const Intersection &isect);
extern vec3f ShadePBS(const Scene *scene,
                      const Ray &ray,
                      const Intersection &isect);

#endif  // OSD_RAYTRACE_SHADER_H
