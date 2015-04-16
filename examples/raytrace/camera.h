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
#ifndef CAMERA_H
#define CAMERA_H

#include "ray.h"
#include "bezier/math.h"

class Camera {
public:
    typedef OsdBezier::vec3d vec3d;

    void BuildCameraFrame(double const eye[3],
                          double const lookat[3],
                          double const up[3],
                          double fov,
                          int width, int height);

    Ray GenerateRay(double u, double v) const;

private:
    // In world space
    vec3d _origin;
    vec3d _corner;
    vec3d _du;
    vec3d _dv;
    double _fov;
};

#endif // CAMERA_H
