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

#define _USE_MATH_DEFINES
#include <cmath>
#include "camera.h"
#include "bezier/math.h"

void Camera::BuildCameraFrame(double const eye[3], double const lookat[3],
                              double const up[3], double fov,
                              int width, int height)
{
    double flen =
        (0.5f * (double)height / tanf(0.5f * (double)(fov * M_PI / 180.0f)));

    vec3d look1 = vec3d(lookat[0] - eye[0],
                        lookat[1] - eye[1],
                        lookat[2] - eye[2]);

    // vcross(u, up1, look1);
    // flip
    vec3d u, v, corner;
    u = cross(look1, vec3d(up[0], up[1], up[2]));
    u.normalize();

    v = cross(look1, u);
    v.normalize();

    double aspect = height/(float)width;
    u = u * aspect;
    v = v / aspect;

    look1.normalize();
    look1[0] = flen * look1[0] + eye[0];
    look1[1] = flen * look1[1] + eye[1];
    look1[2] = flen * look1[2] + eye[2];
    corner[0] = look1[0] - 0.5f * (width * u[0] + height * v[0]);
    corner[1] = look1[1] - 0.5f * (width * u[1] + height * v[1]);
    corner[2] = look1[2] - 0.5f * (width * u[2] + height * v[2]);

    // Store intermediate
    _origin = eye;
    _corner = corner;
    _du = u;
    _dv = v;
    _fov = fov;
}

Ray Camera::GenerateRay(double u, double v) const {

    using namespace OsdBezier;
    vec3f dir;

    dir[0] = (_corner[0] + u * _du[0] + v * _dv[0]) - _origin[0];
    dir[1] = (_corner[1] + u * _du[1] + v * _dv[1]) - _origin[1];
    dir[2] = (_corner[2] + u * _du[2] + v * _dv[2]) - _origin[2];
    dir.normalize();

    vec3f org;
    org[0] = _origin[0];
    org[1] = _origin[1];
    org[2] = _origin[2];

    Ray ray;
    ray.org = org;
    ray.dir = dir;
    ray.depth = 0;

    return ray;
}
