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

#ifndef RAY_H
#define RAY_H

#include "common.h"

struct Ray {
    vec3f org;
    vec3f dir;
    vec3f invDir;
    int dirSign[3];
    int depth;
    bool hasDifferential;

    vec3f dDdx;
    vec3f dDdy;
};

struct Intersection {
    float t;
    float u;
    float v;
    unsigned int faceID;
    unsigned int matID;

    // patch info
    unsigned int patchID;
    unsigned int level;
    unsigned int clipLevel;

    // for measurements
    float        eps;
    unsigned int maxLevel;

    // triangle
    unsigned int f0;
    unsigned int f1;
    unsigned int f2;

    vec3f position;
    vec3f geometricNormal;
    vec3f normal;
    vec3f tangent;
    vec3f binormal;
    float texcoord[2];
};


#endif // RAY_H
