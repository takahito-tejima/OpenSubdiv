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

//----------------------------------------------------------
// Patches.VertexGregoryBasis
//----------------------------------------------------------
#ifdef OSD_PATCH_VERTEX_GREGORY_BASIS_SHADER

layout(location = 0) in vec4 position;
OSD_USER_VARYING_ATTRIBUTE_DECLARE

out block {
    ControlVertex v;
    OSD_USER_VARYING_DECLARE
} outpt;

void main()
{
    outpt.v.position = position;
    OSD_PATCH_CULL_COMPUTE_CLIPFLAGS(position);
    OSD_USER_VARYING_PER_VERTEX();
}

#endif

//----------------------------------------------------------
// Patches.TessControlGregoryBasis
//----------------------------------------------------------
#ifdef OSD_PATCH_TESS_CONTROL_GREGORY_BASIS_SHADER

in block {
    ControlVertex v;
    OSD_USER_VARYING_DECLARE
} inpt[];

out block {
    OsdPerPatchVertexGregoryBasis v;
    OSD_USER_VARYING_DECLARE
} outpt[20];

layout(vertices = 20) out;

void main()
{
    vec3 cv = inpt[gl_InvocationID].v.position.xyz;

    ivec3 patchParam = OsdGetPatchParam(OsdGetPatchIndex(gl_PrimitiveID));
    OsdComputePerPatchVertexGregoryBasis(patchParam, gl_InvocationID, cv, outpt[gl_InvocationID].v);

    OSD_USER_VARYING_PER_CONTROL_POINT(gl_InvocationID, gl_InvocationID);

    if (gl_InvocationID == 0) {
        vec4 tessLevelOuter = vec4(0);
        vec2 tessLevelInner = vec2(0);

        OSD_PATCH_CULL(20);

        OsdGetTessLevels(inpt[0].v.position.xyz, inpt[15].v.position.xyz,
                         inpt[10].v.position.xyz, inpt[5].v.position.xyz,
                         patchParam, tessLevelOuter, tessLevelInner);

        gl_TessLevelOuter[0] = tessLevelOuter[0];
        gl_TessLevelOuter[1] = tessLevelOuter[1];
        gl_TessLevelOuter[2] = tessLevelOuter[2];
        gl_TessLevelOuter[3] = tessLevelOuter[3];

        gl_TessLevelInner[0] = tessLevelInner[0];
        gl_TessLevelInner[1] = tessLevelInner[1];
    }
}

#endif

//----------------------------------------------------------
// Patches.TessEvalGregoryBasis
//----------------------------------------------------------
#ifdef OSD_PATCH_TESS_EVAL_GREGORY_BASIS_SHADER

layout(quads) in;
layout(OSD_SPACING) in;

in block {
    OsdPerPatchVertexGregoryBasis v;
    OSD_USER_VARYING_DECLARE
} inpt[];

out block {
    OutputVertex v;
    OSD_USER_VARYING_DECLARE
} outpt;

void main()
{
    vec3 P = vec3(0), dPu = vec3(0), dPv = vec3(0);
    vec3 dPuu = vec3(0), dPuv = vec3(0), dPvv = vec3(0);

    vec3 cv[20];
    for (int i = 0; i < 20; ++i) {
        cv[i] = inpt[i].v.P;
    }

    vec2 UV = gl_TessCoord.xy;
    ivec3 patchParam = inpt[0].v.patchParam;
    OsdEvalPatchGregory(patchParam, UV, cv, P, dPu, dPv, dPuu, dPuv, dPvv);
    int level = OsdGetPatchFaceLevel(patchParam);
    dPu *= level;
    dPv *= level;
    dPuu *= level*level;
    dPuv *= level*level;
    dPvv *= level*level;
    vec3 n = cross(dPu, dPv);
    vec3 N = normalize(n);

    // all code below here is client code
    outpt.v.position = OsdModelViewMatrix() * vec4(P, 1.0f);
    outpt.v.normal = (OsdModelViewMatrix() * vec4(N, 0.0f)).xyz;
    outpt.v.tangent = (OsdModelViewMatrix() * vec4(dPu, 0.0f)).xyz;
    outpt.v.bitangent = (OsdModelViewMatrix() * vec4(dPv, 0.0f)).xyz;

#ifdef OSD_COMPUTE_NORMAL_DERIVATIVES
    // first and second fundamental forms, curvatures
    float E = dot(dPu, dPu);
    float F = dot(dPu, dPv);
    float G = dot(dPv, dPv);

    float e = dot(N, dPuu);
    float f = dot(N, dPuv);
    float g = dot(N, dPvv);

    outpt.v.Nu = (f*F-e*G)/(E*G-F*F) * dPu + (e*F-f*E)/(E*G-F*F) * dPv;
    outpt.v.Nv = (g*F-f*G)/(E*G-F*F) * dPu + (f*F-g*E)/(E*G-F*F) * dPv;
    outpt.v.Kcurvature = (e*g - f*f)/(E*G - F*F);
    outpt.v.Hcurvature = 0.5 * (E*g - 2*F*f - G*e) / (E*G - F*F);
#endif

    // scale tangents
    /* int level = OsdGetPatchFaceLevel(patchParam); */
    /* outpt.v.tangent *= level; */
    /* outpt.v.bitangent *= level; */

    outpt.v.tessCoord = UV;
    outpt.v.patchCoord = OsdInterpolatePatchCoord(UV, patchParam);

    OSD_USER_VARYING_PER_EVAL_POINT(UV, 0, 5, 15, 10);

    OSD_DISPLACEMENT_CALLBACK;

    gl_Position = OsdProjectionMatrix() * outpt.v.position;
}

#endif
