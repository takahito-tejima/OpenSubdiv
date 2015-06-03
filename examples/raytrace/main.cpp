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

#if defined(__APPLE__)
    #if defined(OSD_USES_GLEW)
        #include <GL/glew.h>
    #else
        #include <OpenGL/gl3.h>
    #endif
    #define GLFW_INCLUDE_GL3
    #define GLFW_NO_GLU
#else
    #include <stdlib.h>
    #include <GL/glew.h>
    #if defined(WIN32)
        #include <GL/wglew.h>
    #endif
#endif

#include <GLFW/glfw3.h>
GLFWwindow* g_window=0;
GLFWmonitor* g_primary=0;

#include <osd/cpuVertexBuffer.h>
#include <osd/cpuEvaluator.h>

#include <far/stencilTableFactory.h>
#include <far/patchTableFactory.h>

#include "../../regression/common/far_utils.h"
#include "../../regression/common/shape_utils.h"
#include "../common/stopwatch.h"
#include "../common/simple_math.h"
#include "../common/glHud.h"
#include "../common/glUtils.h"

#include "scene.h"

#include <cfloat>
#include <vector>
#include <fstream>
#include <sstream>

static const char *s_VS =
    "#version 410\n"
    "in vec2 position;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(position.x, position.y, 0, 1);\n"
    "  uv = (-position.yx+vec2(1))*0.5;\n"
    "}\n";
static const char *s_FS =
    "#version 410\n"
    "in vec2 uv;\n"
    "out vec4 outColor;\n"
    "uniform sampler2D tex;\n"
    "uniform float exposure;\n"
    "void main()\n"
    "{\n"
    "  ivec2 texUV = ivec2(textureSize(tex, 0)*uv);\n"
    "  vec4 c = vec4(0.1, 0.1, 0.1, 0);\n"
    "  vec4 cs = texelFetch(tex, texUV, 0);\n"
    "  vec4 c0 = texelFetch(tex, ivec2(texUV.x & ~7, texUV.y & ~7), 0);\n"
    "  vec4 c1 = texelFetch(tex, ivec2(texUV.x & ~3, texUV.y & ~3), 0);\n"
    "  vec4 c2 = texelFetch(tex, ivec2(texUV.x & ~1, texUV.y & ~1), 0);\n"
    "  c0.a = min(1, c0.a); \n"
    "  c1.a = min(1, c1.a); \n"
    "  c2.a = min(1, c2.a); \n"
    "  c = mix(c, c0, c0.a);\n"
    "  c = mix(c, c1, c1.a);\n"
    "  c = mix(c, c2, c2.a);\n"
    "  c = mix(c, cs, min(1, cs.a));\n"
    "  outColor = c;\n"
    "  outColor.rgb = pow(exposure*outColor.rgb/max(1, cs.a), vec3(0.454545));\n"
    "}\n";
static const char *s0_FS =
    "#version 410\n"
    "in vec2 uv;\n"
    "out vec4 outColor;\n"
    "uniform sampler2D tex;\n"
    "uniform float exposure;\n"
    "void main()\n"
    "{\n"
    "  ivec2 texUV = ivec2(textureSize(tex, 0)*uv);\n"
    "  vec4 c = vec4(0.1, 0.1, 0.1, 0);\n"
    "  vec4 cs = texelFetch(tex, texUV, 0);\n"
    "  outColor = mix(c, cs, min(1, cs.a));\n"
    "  outColor.rgb = pow(exposure*outColor.rgb/max(1, cs.a), vec3(0.454545));\n"
    "}\n";

static const char *s_VS_BVH =
    "#version 410\n"
    "in vec3 minPos;\n"
    "in vec3 maxPos;\n"
    "in int flag;\n"
    "out vec3 gMinPos;\n"
    "out vec3 gMaxPos;\n"
    "out vec4 gColor;\n"
    "void main() {\n"
    "  gMinPos = minPos;\n"
    "  gMaxPos = maxPos;\n"
    "  gColor = (flag==1) ? vec4(0.0, 0.8, 0.0, 0.5) : vec4(0.1, 0.3, 0.1, 0.5);\n"
    "}\n";

static const char *s_GS_BVH =
    "#version 410\n"
    "layout(points) in;\n"
    "layout(line_strip, max_vertices = 24) out;\n"
    "in vec3 gMinPos[];\n"
    "in vec3 gMaxPos[];\n"
    "in vec4 gColor[];\n"
    "out vec4 fColor;\n"
    "uniform mat4 ModelViewProjectionMatrix;\n"
    "void emit(vec3 p0, vec3 p1){\n"
    "  gl_Position = ModelViewProjectionMatrix * vec4(p0, 1);\n"
    "  EmitVertex();\n"
    "  gl_Position = ModelViewProjectionMatrix * vec4(p1, 1);\n"
    "  EmitVertex();\n"
    "  EndPrimitive();\n"
    "}\n"
    "void main() {\n"
    "  vec3 p0 = gMinPos[0];\n"
    "  vec3 p1 = gMaxPos[0];\n"
    "  fColor = gColor[0];\n"
    "  emit(vec3(p0.x, p0.y, p0.z), vec3(p1.x, p0.y, p0.z));\n"
    "  emit(vec3(p0.x, p0.y, p1.z), vec3(p1.x, p0.y, p1.z));\n"
    "  emit(vec3(p0.x, p1.y, p0.z), vec3(p1.x, p1.y, p0.z));\n"
    "  emit(vec3(p0.x, p1.y, p1.z), vec3(p1.x, p1.y, p1.z));\n"
    "  emit(vec3(p0.x, p0.y, p0.z), vec3(p0.x, p1.y, p0.z));\n"
    "  emit(vec3(p0.x, p0.y, p1.z), vec3(p0.x, p1.y, p1.z));\n"
    "  emit(vec3(p1.x, p0.y, p0.z), vec3(p1.x, p1.y, p0.z));\n"
    "  emit(vec3(p1.x, p0.y, p1.z), vec3(p1.x, p1.y, p1.z));\n"
    "  emit(vec3(p0.x, p0.y, p0.z), vec3(p0.x, p0.y, p1.z));\n"
    "  emit(vec3(p0.x, p1.y, p0.z), vec3(p0.x, p1.y, p1.z));\n"
    "  emit(vec3(p1.x, p0.y, p0.z), vec3(p1.x, p0.y, p1.z));\n"
    "  emit(vec3(p1.x, p1.y, p0.z), vec3(p1.x, p1.y, p1.z));\n"
    "}\n";

static const char *s_FS_BVH =
    "#version 410\n"
    "in vec4 fColor;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "  outColor = fColor;\n"
    "}\n";

static const char *s_VS_Debug =
    "#version 410\n"
    "in vec2 position;\n"
    "out vec2 uv;\n"
    "uniform float debugScale=0.01;\n"
    "uniform vec2 debugScope;\n"
    "void main() {\n"
    "  vec2 pos = position.xy * 0.25 + vec2(0.75, -0.75);\n"
    "  uv = (-position.yx*debugScale + debugScope +vec2(1))*0.5;\n"
    "  gl_Position = vec4(pos.x, pos.y, 0, 1);\n"
    "}\n";

enum HudCheckBox { kHUD_CB_DISPLAY_BVH,
                   kHUD_CB_BLOCK_FILL,
                   kHUD_CB_WATERTIGHT,
                   kHUD_CB_CROP_UV,
                   kHUD_CB_BEZIER_CLIP,
                   kHUD_CB_ANIMATE,
                   kHUD_CB_DEBUG,
                   kHUD_CB_TRIANGLE,
                   kHUD_CB_RAYDIFFEPSILON,
                   kHUD_CB_CONSERVATIVE_TEST,
                   kHUD_CB_DIRECT_BILINEAR,
                   kHUD_CB_USE_SINGLE_CREASE_PATCH };

static void setCamera();

int g_currentShape = 0;

// GUI variables
//int   g_displayStyle = Scene::SHADED, //Scene::PATCH_TYPE,
int   g_displayStyle = Scene::PATCH_TYPE,
      g_drawBVH = false,
      g_blockFill = true,
      g_mbutton[3] = {0, 0, 0},
      g_partitioning = 1,
      g_running = 1;

float g_rotate[2] = {0, 0},
      g_dolly = 5,
      g_pan[2] = {0, 0},
      g_center[3] = {0, 0, 0},
      g_size = 0;

int   g_prev_x = 0,
      g_prev_y = 0;

int   g_frameBufferWidth = 1024,
    g_frameBufferHeight = 1024;

int   g_width = 1024,
      g_height = 1024;
std::vector<float> g_image;
int g_step = 2;
int g_stepIndex = 0;


GLhud g_hud;

// performance
float g_cpuTime = 0;
float g_gpuTime = 0;
Stopwatch g_fpsTimer;
Stopwatch g_renderTimer;
float g_hbrTime = 0;
float g_farTime = 0;
float g_subdivTime = 0;
float g_convertTime = 0;
float g_bvhTime = 0;
float g_renderTime = 0;
float g_exposure = 1.0f;

// geometry
std::vector<float> g_orgPositions;

OpenSubdiv::Osd::CpuVertexBuffer *g_cpuVertexBuffer = NULL;
OpenSubdiv::Far::PatchTable *g_patchTable = NULL;
OpenSubdiv::Far::StencilTable const * g_stencilTable = NULL;
OpenSubdiv::Far::TopologyRefiner * g_topologyRefiner = NULL;


int g_level = 2;
int g_intersectKernel = 1;
int g_watertight = 1;
int g_cropUV = 0;
int g_bezierClip = 1;
int g_debug = 0;
float g_debugScale = 0.01f;
int g_debugScope[2] = { g_width/2, g_height/2 };
float g_uvMargin = 0.0f;

int g_epsLevel = 4;//4->16
int g_maxLevel = 16;//10->32
int g_minLeafPrimitives = 2;
int g_useTriangle = 0;

int g_useRayDiffEpsilon = 1;
int g_conservativeTest = 1;
int g_directBilinear = 0;
int g_useSingleCreasePatch = 1;

int g_animate = 0;
int g_frame = 0;

Scene::BackgroundMode g_backgroundType = Scene::GRADATION;

struct Transform {
    float ModelViewMatrix[16];
    float ProjectionMatrix[16];
    float ModelViewProjectionMatrix[16];
} g_transformData;

GLuint g_vao = 0;
GLuint g_vaoBVH = 0;
GLuint g_vbo = 0;
GLuint g_programSimpleFill = 0;
GLuint g_programBlockFill = 0;
GLuint g_programBVH = 0;
GLuint g_programDebug = 0;

float g_eye[] = { 0, 0, 5, 1};
float g_lookat[] = {0, 0, 0, 1};
float g_up[] = {0, 1, 0, 0};

Scene g_scene;
std::vector<int> g_ptexIDToFaceIDMapping;
std::vector<unsigned short> g_materialBinds;

//------------------------------------------------------------------------------

#include "init_shapes.h"

//------------------------------------------------------------------------------
static void
setup() {
    int width = g_width;
    int height = g_height;
    g_image.clear();
    g_image.resize(width*height*4);

    double fov = 45.0f;
    g_scene.SetCamera(width, height, fov, g_image, g_eye, g_lookat, g_up);

    Scene::Config config;
    config.intersectKernel = g_intersectKernel;
    config.uvMargin = g_uvMargin;
    config.cropUV = g_cropUV;
    config.bezierClip = g_bezierClip;
    config.epsLevel = g_epsLevel;
    config.maxLevel = g_maxLevel;
    config.useTriangle = g_useTriangle;
    config.useRayDiffEpsilon = g_useRayDiffEpsilon;
    config.conservativeTest = g_conservativeTest;
    config.directBilinear = g_directBilinear;
    config.step = g_step;

    g_scene.SetConfig(config);
    g_scene.SetBackgroudMode(g_backgroundType);
}

static void
startRender() {
    setup();
    g_stepIndex = g_step*g_step;
    g_renderTimer.Start();
}

static void
report() {
    setup();
    g_scene.RenderReport();

    g_stepIndex = g_step*g_step;
    g_renderTimer.Start();
}

static void
setCamera()
{
    // prepare view matrix
    double aspect = g_width/(double)g_height;
    identity(g_transformData.ModelViewMatrix);
    translate(g_transformData.ModelViewMatrix, -g_pan[0], -g_pan[1], -g_dolly);
    rotate(g_transformData.ModelViewMatrix, g_rotate[1], 1, 0, 0);
    rotate(g_transformData.ModelViewMatrix, g_rotate[0], 0, 1, 0);
    rotate(g_transformData.ModelViewMatrix, -90, 1, 0, 0);
    perspective(g_transformData.ProjectionMatrix,
                45.0f, (float)aspect, 0.01f, 500.0f);
    multMatrix(g_transformData.ModelViewProjectionMatrix,
               g_transformData.ModelViewMatrix,
               g_transformData.ProjectionMatrix);

    float invView[16];
    inverseMatrix(invView, g_transformData.ModelViewMatrix);

    g_eye[0] = g_eye[1] = g_eye[2] = 0;
    apply(g_eye, invView);

    g_lookat[0] = g_lookat[1] = 0;
    g_lookat[2] = -5;
    apply(g_lookat, invView);

    g_up[0] = g_up[2] = 1;
    g_up[1] = 0;
    apply(g_up, invView);


    startRender();
}

static void
dumpCamera()
{
  const char *filename = "camera.dat";

  FILE* fp = fopen(filename, "w");
  if (!fp) {
    fprintf(stderr, "Failed tp open file: %s\n", filename);
    return;
  }

  fprintf(fp, "%f %f %f\n", g_pan[0], g_pan[1], g_dolly);
  fprintf(fp, "%f %f\n", g_rotate[0], g_rotate[1]);

  fclose(fp);

  printf("camera data dumped.\n"); fflush(stdout);
}

static void
loadCamera()
{
  const char *filename = "camera.dat";
  FILE* fp = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "Failed tp open file: %s\n", filename);
    return;
  }

  fscanf(fp, "%f %f %f\n", &g_pan[0], &g_pan[1], &g_dolly);
  fscanf(fp, "%f %f\n", &g_rotate[0], &g_rotate[1]);
  fclose(fp);
  printf("camera data loaded.\n"); fflush(stdout);
}

static void
updateGeom() {

    int nverts = (int)g_orgPositions.size() / 3;

    std::vector<float> vertex;
    vertex.reserve(nverts*3);

    //float r = g_animate ? sin(g_frame*0.1f) : 0.0f;
    const float *pp = &g_orgPositions[0];
    for (int i = 0; i < nverts; ++i) {
        float x = pp[0];
        float y = pp[1];
        float z = pp[2];
        //float ct = cos(y * r);
        //float st = sin(y * r);
        // vertex.push_back(x * ct + z * st);
        // vertex.push_back(y);
        // vertex.push_back(- x * st + z * ct);
        vertex.push_back(x);
        vertex.push_back(y);
        vertex.push_back(z);
        pp += 3;
    }

    g_cpuVertexBuffer->UpdateData(&vertex[0], 0, nverts);

    Stopwatch s;
    s.Start();


    int numControlVertices = g_stencilTable->GetNumControlVertices();
    OpenSubdiv::Osd::BufferDescriptor vertexSrcDesc(0, 3, 3);
    OpenSubdiv::Osd::BufferDescriptor vertexDstDesc(3*numControlVertices, 3, 3);

    OpenSubdiv::Osd::CpuEvaluator::EvalStencils(
        g_cpuVertexBuffer, vertexSrcDesc,
        g_cpuVertexBuffer, vertexDstDesc,
        g_stencilTable);

    s.Stop();
    g_subdivTime = s.GetElapsed() * 1000.0f;

    s.Start();

    g_scene.GetMesh().BezierConvert(g_cpuVertexBuffer->BindCpuBuffer(),
                                    g_patchTable);

    g_scene.GetMesh().AssignMaterialIDs(g_ptexIDToFaceIDMapping,
                                        g_materialBinds);

    s.Stop();
    g_convertTime = s.GetElapsed() * 1000.0f;

    s.Start();
    g_scene.BuildBVH(g_minLeafPrimitives);
    s.Stop();

    g_bvhTime = s.GetElapsed() * 1000.0f;

    g_scene.BuildVBO();

    startRender();
}

//------------------------------------------------------------------------------
static void
createOsdMesh( const std::string &shapeStr, int level ){

    GLUtils::CheckGLErrors("create osd enter");
    // generate Hbr representation from "obj" description

    Stopwatch s;

    // create refiner

    Shape * shape = Shape::parseObj(shapeStr.c_str(), kCatmark, 1, true);
    //    shape->addGroundPlane(5.0f, -0.5f);
    if (g_topologyRefiner) delete g_topologyRefiner;

    if (shape->mtlbind.empty()) {
        g_scene.GetMesh().SetMaterial(0, Material());
    } else {
        for (int i = 0; i < (int)shape->mtls.size(); ++i) {
            Shape::material const *m = shape->mtls[i];
            Material mat;
            mat.diffuse = vec3f(m->kd[0], m->kd[1], m->kd[2]);
            mat.reflection = vec3f(m->ks[0], m->ks[1], m->ks[2]);
            mat.refraction = vec3f(1.0f-m->d);
            mat.reflectionGlossiness = m->sharpness;
            mat.refractionGlossiness = 1;
            mat.fresnel = true;
            mat.ior = 1.0;//m->ni;

            g_scene.GetMesh().SetMaterial(i, mat);
        }
        for (int i = 0; i < (int)shape->mtlbind.size(); ++i){
        }
    }

    {
        OpenSubdiv::Sdc::SchemeType type = GetSdcType(*shape);
        OpenSubdiv::Sdc::Options options = GetSdcOptions(*shape);

        OpenSubdiv::Far::TopologyRefinerFactory<Shape>::Options opt(type, options);

        g_topologyRefiner = OpenSubdiv::Far::TopologyRefinerFactory<Shape>::Create(
            *shape, opt);

        assert(g_topologyRefiner);
    }

    // refine
    {
        OpenSubdiv::Far::TopologyRefiner::AdaptiveOptions options(level);
        options.useSingleCreasePatch = g_useSingleCreasePatch;
        g_topologyRefiner->RefineAdaptive(options);
    }

    // patch table
    delete g_patchTable;
    OpenSubdiv::Far::PatchTableFactory::Options options;
    options.useSingleCreasePatch = g_useSingleCreasePatch;
    options.endCapType = OpenSubdiv::Far::PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS;
    options.maxIsolationLevel = level;
    g_patchTable = OpenSubdiv::Far::PatchTableFactory::Create(*g_topologyRefiner, options);

    delete g_stencilTable;
    {
        OpenSubdiv::Far::StencilTableFactory::Options options;
        options.generateOffsets = true;
        options.generateIntermediateLevels = true;

        g_stencilTable = OpenSubdiv::Far::StencilTableFactory::Create(*g_topologyRefiner, options);
        assert(g_stencilTable);
    }
    // append local points to stencils
    {
        if (OpenSubdiv::Far::StencilTable const *vertexStencilsWithLocalPoints =
            OpenSubdiv::Far::StencilTableFactory::AppendLocalPointStencilTable(
                *g_topologyRefiner,
                g_stencilTable,
                g_patchTable->GetLocalPointStencilTable())) {
            delete g_stencilTable;
            g_stencilTable = vertexStencilsWithLocalPoints;
        }
    }


    // resolve material indices
    OpenSubdiv::Far::PatchParamTable patchParam = g_patchTable->GetPatchParamTable();

    // XXX: need refactoring.
//    g_ptexIDToFaceIDMapping = shape->GetPtexIDToFaceIDMapping();
    g_materialBinds = shape->mtlbind;

    int numVerts = g_stencilTable->GetNumStencils() + g_stencilTable->GetNumControlVertices();
    g_cpuVertexBuffer = OpenSubdiv::Osd::CpuVertexBuffer::Create(3, numVerts);

    g_orgPositions = std::vector<float>(shape->verts);

    // compute model bounding
    float min[3] = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    float max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (size_t i=0; i <g_orgPositions.size()/3; ++i) {
        for(int j=0; j<3; ++j) {
            float v = g_orgPositions[i*3+j];
            min[j] = std::min(min[j], v);
            max[j] = std::max(max[j], v);
        }
    }
    for (int j=0; j<3; ++j) {
        g_center[j] = (min[j] + max[j]) * 0.5f;
        g_size += (max[j]-min[j])*(max[j]-min[j]);
    }
    g_size = sqrtf(g_size);

    for (size_t i=0; i <g_orgPositions.size()/3; ++i) {
        for (int j=0; j<3; ++j) {
            g_orgPositions[i*3+j] =
                (g_orgPositions[i*3+j] - g_center[j])/(0.333*g_size);
        }
    }
    g_center[0] = g_center[1] = g_center[2] = 0;
    g_size = 1.0f;


    setCamera();
    updateGeom();
}

float g_selectedColor[3];
static void
debugTrace(int x, int y) {
    float *p = &g_image[(y*g_width + x)*4];
    g_selectedColor[0] = p[0];
    g_selectedColor[1] = p[1];
    g_selectedColor[2] = p[2];

    //g_traceEnabled = true;
    g_scene.DebugTrace(x + g_step/2.0f, y + g_step/2.0f);
    //g_traceEnabled = false;
}

//------------------------------------------------------------------------------
static void
fitFrame() {

    g_pan[0] = g_pan[1] = 0;
    g_dolly = g_size;

    setCamera();
}

//------------------------------------------------------------------------------
static void
displayBVH() {

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(g_vaoBVH);

    glBindBuffer(GL_ARRAY_BUFFER, g_scene.GetVBO());
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BVHNode), (void*)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BVHNode), (void*)(sizeof(float)*3));
    glVertexAttribIPointer(2, 1, GL_INT, sizeof(BVHNode), (void*)(sizeof(float)*6));

    glUseProgram(g_programBVH);

    glUniformMatrix4fv(0, 1, GL_FALSE, g_transformData.ModelViewProjectionMatrix);

    glDrawArrays(GL_POINTS, 0, g_scene.GetNumBVHNode());

    glUseProgram(0);

    glBindVertexArray(0);

    glDisable(GL_BLEND);

}

//------------------------------------------------------------------------------

static void
display() {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, g_frameBufferWidth, g_frameBufferHeight);

    static GLuint tex = 0;
    if (tex == 0) glGenTextures(1, &tex);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                 g_width, g_height,
                 0, GL_RGBA, GL_FLOAT, &g_image[0]);

    GLuint program = g_blockFill ? g_programBlockFill : g_programSimpleFill;
    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program, "exposure"), g_exposure);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat)*2, (void*)0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (g_debug) {
        glUseProgram(g_programDebug);
        float sy = (g_width/2 - g_debugScope[0]) / float(g_width/2);
        float sx = (g_debugScope[1] - g_height/2) / float(g_height/2);
        glUniform1f(glGetUniformLocation(g_programDebug, "debugScale"), g_debugScale);
        glUniform2f(glGetUniformLocation(g_programDebug, "debugScope"), sx, sy);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glUseProgram(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (g_drawBVH) {
        displayBVH();
    }

    if (g_hud.IsVisible()) {
        g_fpsTimer.Stop();
        double fps = 1.0/g_fpsTimer.GetElapsed();
        g_fpsTimer.Start();

        double rps = g_width*g_height/g_renderTime/1000.0/1000.0;

        g_hud.DrawString(10, -300, "# of patches      : %8d", g_scene.GetMesh().GetNumPatches());

        g_hud.DrawString(10, -220, "Memory            : %8.1f MB (mesh), %8.1f MB (bvh)",
                         g_scene.GetMesh().GetMemoryUsage()/1024.0/1024.0,
                         g_scene.GetMemoryUsage()/1024.0/1024.0);

        g_hud.DrawString(10, -200, "Hbr time          : %8.1f ms", g_hbrTime);
        g_hud.DrawString(10, -180, "Far time          : %8.1f ms", g_farTime);
        g_hud.DrawString(10, -160, "Subdivision time  : %8.1f ms", g_subdivTime);

        g_hud.DrawString(10, -140, "Bspline to Bezier : %8.1f ms", g_convertTime);
        g_hud.DrawString(10,  -80, "BVH build         : %8.1f ms", g_bvhTime);
        if (g_renderTime > 0) {
            g_hud.DrawString(10, -40,  "Render time       : %5.3f s  (%3.1f Mrays/sec)",
                             g_renderTime, rps);
        } else {
            g_hud.DrawString(10, -40,  1, 0, 0, "Render time       : %2.0f%%",
                             100*(1-g_stepIndex/float(g_step*g_step)));
        }
        g_hud.DrawString(10, -20,  "FPS               : %3.1f", fps);


        if (g_debug) {
            g_hud.DrawString(-300, -300,
                             g_selectedColor[0], g_selectedColor[1], g_selectedColor[2],
                             "COLOR : %f %f %f", 
                             g_selectedColor[0], g_selectedColor[1], g_selectedColor[2]);
            g_hud.DrawString(g_debugScope[0]-10, g_debugScope[1]-5, "[ ]");
        }

        g_hud.Flush();
    }


    glfwSwapBuffers(g_window);
    //checkGLErrors("display leave");
}

//------------------------------------------------------------------------------
static void
motion(GLFWwindow *, double dx, double dy) {
    int x=(int)dx, y=(int)dy;

    if (g_hud.MouseCapture()) {
        // check gui (for slider)
        g_hud.MouseMotion(x, y);
    } else if (g_debug) {
        //
    } else if (g_mbutton[0] && !g_mbutton[1] && !g_mbutton[2]) {
        // orbit
        g_rotate[0] += (x - g_prev_x);
        g_rotate[1] += (y - g_prev_y);
        setCamera();
    } else if (!g_mbutton[0] && !g_mbutton[1] && g_mbutton[2]) {
        // pan
        g_pan[0] -= 0.5*g_dolly*(x - g_prev_x)/g_width;
        g_pan[1] += 0.5*g_dolly*(y - g_prev_y)/g_height;
        setCamera();
    } else if ((g_mbutton[0] && !g_mbutton[1] && g_mbutton[2]) or
               (!g_mbutton[0] && g_mbutton[1] && !g_mbutton[2])) {
        // dolly
        g_dolly -= g_dolly*0.01f*(x - g_prev_x);
        if(g_dolly <= 0.01) g_dolly = 0.01f;
        setCamera();
    }

    g_prev_x = x;
    g_prev_y = y;
}

//------------------------------------------------------------------------------
static void
mouse(GLFWwindow *, int button, int state, int mods) {

    if (state == GLFW_RELEASE) {
        g_hud.MouseRelease();
        g_mbutton[0] = g_mbutton[1] = g_mbutton[2] = 0;
        g_step = 2;
        startRender();
    }

    if (state == GLFW_PRESS) {
        g_step = 8;
    }

    if (button == 0 && state == GLFW_PRESS && g_hud.MouseClick(g_prev_x, g_prev_y)) {
        display();
        return;
    }

    if (mods & GLFW_MOD_SHIFT) {
        // for mac
        button = 2;
    }
    if (button < 3) {
        g_mbutton[button] = (state == GLFW_PRESS);
    }

    if (g_debug && button == 0 && state == GLFW_PRESS) {
        float x = g_prev_x/float(g_width);
        float y = g_prev_y/float(g_height);
        x = (((x - 0.75)/0.25)*2 - 1);
        y = (((y - 0.75)/0.25)*2 - 1);
        if (x >= -1 and y >=-1 and x <= 1 and y <= 1) {
            x = (g_width - g_debugScope[0]) - (x*g_width/2)*g_debugScale;
            y = g_debugScope[1] + (y*g_height/2)*g_debugScale;

            debugTrace((int)y, (int)x);
        } else {
            g_debugScope[0] = g_prev_x;
            g_debugScope[1] = g_prev_y;
        }
        display();
    }

}

//------------------------------------------------------------------------------
static void
uninitGL() {

    glDeleteVertexArrays(1, &g_vao);
    glDeleteVertexArrays(1, &g_vaoBVH);
    glDeleteBuffers(1, &g_vbo);

    delete g_stencilTable;
    delete g_patchTable;
}

//------------------------------------------------------------------------------
static void
reshape(GLFWwindow *, int width, int height) {

    g_width = width;
    g_height = height;

    g_frameBufferWidth = width;
    g_frameBufferHeight = height;

    // window size might not match framebuffer size on a high DPI display
    glfwGetWindowSize(g_window, &g_width, &g_height);

    g_hud.Rebuild(g_width, g_height, g_frameBufferWidth, g_frameBufferHeight);

    setCamera();
    startRender();
}

//------------------------------------------------------------------------------
void windowClose(GLFWwindow*) {
    g_running = false;
}

//------------------------------------------------------------------------------

static void
keyboardChar(GLFWwindow *, unsigned int codepoint)
{
    char key = (char)(codepoint & 0x7f);//unicode to ascii

    if (g_hud.KeyDown(tolower(key))) return;

    switch (key) {
        case ' ': startRender(); break;
        case 'q': g_running = 0; break;
        case 'f': fitFrame(); break;
        case '+':
        case 'G': dumpCamera(); break;
        case 'g': loadCamera(); setCamera(); break;
        case '#': {
            if (g_backgroundType == Scene::GRADATION) {
              g_backgroundType = Scene::WHITE;
            } else if (g_backgroundType == Scene::WHITE) {
              g_backgroundType = Scene::BLACK;
            } else if (g_backgroundType == Scene::BLACK) {
              g_backgroundType = Scene::ENVMAP;
            } else if (g_backgroundType == Scene::ENVMAP) {
              g_backgroundType = Scene::GRADATION;
            }
            g_scene.SetBackgroudMode(g_backgroundType);
            startRender(); break;
          }
        case '*': report(); break;
    }
}
static void
keyboard(GLFWwindow *, int key, int /* scancode */, int event, int /* mods */)
{
    if (event == GLFW_RELEASE) return;

    const float panStep = g_dolly*0.0001;
    switch (key) {
    case GLFW_KEY_UP:
        g_pan[1] -= panStep; setCamera(); break;
    case GLFW_KEY_DOWN:
        g_pan[1] += panStep; setCamera(); break;
    case GLFW_KEY_LEFT:
        g_pan[0] += panStep; setCamera(); break;
    case GLFW_KEY_RIGHT:
        g_pan[0] -= panStep; setCamera(); break;

    case GLFW_KEY_ESCAPE: g_hud.SetVisible(!g_hud.IsVisible()); display(); break;
    }
}

//------------------------------------------------------------------------------
static void
rebuildOsdMesh()
{
    createOsdMesh( g_defaultShapes[ g_currentShape ].data, g_level );
}

static void
callbackLevel(int l)
{
    g_level = l;
    rebuildOsdMesh();
}

static void
callbackModel(int m)
{
    if (m < 0)
        m = 0;

    if (m >= (int)g_defaultShapes.size())
        m = (int)g_defaultShapes.size() - 1;

    g_currentShape = m;
    rebuildOsdMesh();
}

static void
callbackDisplayStyle(int b)
{
    g_displayStyle = b;
    g_scene.SetShadeMode((Scene::ShadeMode)b);

    startRender();
}

static void
callbackIntersect(int b)
{
    g_intersectKernel = b;

    startRender();
}

static void
callbackSlider(float value, int data)
{
    if (data == -2) {
        g_epsLevel = (int)ceil(value);
        startRender();
    } else if (data == -1) {
        g_maxLevel = (int)ceil(value);
        startRender();
    } else if (data == 0) {
        g_uvMargin = value;
        startRender();
    } else if (data == 3) {
        if (g_minLeafPrimitives != value) {
            g_minLeafPrimitives = value;
            updateGeom();
        }
    } else if (data == 4) {
        g_exposure = value;
    }
}

static void
callbackCheckBox(bool checked, int button)
{
    switch (button) {
    case kHUD_CB_DISPLAY_BVH:
        g_drawBVH = checked;
        break;
    case kHUD_CB_BLOCK_FILL:
        g_blockFill = checked;
        break;
    case kHUD_CB_ANIMATE:
        g_animate = checked;
        updateGeom();
        break;
    case kHUD_CB_WATERTIGHT:
        g_watertight = checked;
        updateGeom();
        break;
    case kHUD_CB_CROP_UV:
        g_cropUV = checked;
        startRender();
        break;
    case kHUD_CB_BEZIER_CLIP:
        g_bezierClip = checked;
        startRender();
        break;
    case kHUD_CB_DEBUG:
        g_debug = checked;
        break;
    case kHUD_CB_TRIANGLE:
        g_useTriangle = checked;
        startRender();
        break;
    case kHUD_CB_RAYDIFFEPSILON:
        g_useRayDiffEpsilon = checked;
        startRender();
        break;
    case kHUD_CB_CONSERVATIVE_TEST:
        g_conservativeTest = checked;
        startRender();
        break;
    case kHUD_CB_DIRECT_BILINEAR:
        g_directBilinear = checked;
        startRender();
        break;
    case kHUD_CB_USE_SINGLE_CREASE_PATCH:
        g_useSingleCreasePatch = checked;
        rebuildOsdMesh();
        break;
    }
    display();
}

static void
initHUD()
{
    // window size might not match framebuffer size on a high DPI display
    glfwGetWindowSize(g_window, &g_width, &g_height);
    g_hud.Init(g_width, g_height, g_frameBufferWidth, g_frameBufferHeight);

    int y = 10;
    g_hud.AddCheckBox("Show BVH (B)", g_drawBVH != 0,
                      10, y, callbackCheckBox, kHUD_CB_DISPLAY_BVH, 'b');y+=20;
    g_hud.AddCheckBox("Block Fill (K)", g_blockFill != 0,
                      10, y, callbackCheckBox, kHUD_CB_BLOCK_FILL, 'k');y+=20;

    g_hud.AddCheckBox("Watertight (C)", g_watertight != 0,
                      10, y, callbackCheckBox, kHUD_CB_WATERTIGHT, 'c');y+=20;

    g_hud.AddCheckBox("Crop UV (U)", g_cropUV != 0,
                      10, y, callbackCheckBox, kHUD_CB_CROP_UV, 'u');y+=20;

    g_hud.AddCheckBox("Bezier Clip (J)", g_bezierClip != 0,
                      10, y, callbackCheckBox, kHUD_CB_BEZIER_CLIP, 'j');y+=20;

    g_hud.AddCheckBox("Animate vertices (M)", g_animate != 0,
                      10, y, callbackCheckBox, kHUD_CB_ANIMATE, 'm');y+=20;
    g_hud.AddCheckBox("Debug (D)", g_debug != 0,
                      10, y, callbackCheckBox, kHUD_CB_DEBUG, 'd');y+=20;

    g_hud.AddCheckBox("Intersect Triangle (E)", g_useTriangle != 0,
                      10, y, callbackCheckBox, kHUD_CB_TRIANGLE, 'e');y+=20;

    g_hud.AddCheckBox("RayDiff Epsilon (R)", g_useRayDiffEpsilon != 0,
                      10, y, callbackCheckBox, kHUD_CB_RAYDIFFEPSILON, 'r');y+=20;

    g_hud.AddCheckBox("Conservative Test (Y)", g_conservativeTest != 0,
                      10, y, callbackCheckBox, kHUD_CB_CONSERVATIVE_TEST, 'y');y+=20;

    g_hud.AddCheckBox("Direct bilinear (X)", g_directBilinear != 0,
                      10, y, callbackCheckBox, kHUD_CB_DIRECT_BILINEAR, 'x');y+=20;

    g_hud.AddCheckBox("Single-Crease Patch (S)", g_useSingleCreasePatch != 0,
                      10, y, callbackCheckBox, kHUD_CB_USE_SINGLE_CREASE_PATCH, 's');y+=20;

    g_hud.AddSlider("Epsilon Level", 1, 16, g_epsLevel,
                    10, y, 20, true, callbackSlider, -2);y+=30;
    g_hud.AddSlider("Max Level", 0, 16, g_maxLevel,
                    10, y, 20, true, callbackSlider, -1);y+=30;
    g_hud.AddSlider("# of leaf", 2, 16, g_minLeafPrimitives,
                    10, y, 20, true, callbackSlider, 3);y+=30;

    g_hud.AddSlider("UV Margin", 0, 0.01, g_uvMargin,
                    10, y, 20, false, callbackSlider, 0);y+=30;

    g_hud.AddSlider("Exposure", 0, 2.0, g_exposure,
                    10, y, 20, false, callbackSlider, 4);y+=30;

    int kernel_pulldown = g_hud.AddPullDown("Intersect Kernel (I)", 400, 10, 200, callbackIntersect, 'i');
    g_hud.AddPullDownButton(kernel_pulldown, "float", 0, g_intersectKernel == 0);
    g_hud.AddPullDownButton(kernel_pulldown, "sse", 1, g_intersectKernel == 1);
    g_hud.AddPullDownButton(kernel_pulldown, "double", 2, g_intersectKernel == 2);

    int shading_pulldown = g_hud.AddPullDown("Shading (W)", 200, 10, 250, callbackDisplayStyle, 'w');
    g_hud.AddPullDownButton(shading_pulldown, "Shaded", Scene::SHADED,
                            g_displayStyle==Scene::SHADED);
    g_hud.AddPullDownButton(shading_pulldown, "Patch coord", Scene::PATCH_COORD,
                            g_displayStyle==Scene::PATCH_COORD);
    g_hud.AddPullDownButton(shading_pulldown, "Patch color", Scene::PATCH_TYPE,
                            g_displayStyle==Scene::PATCH_TYPE);
    g_hud.AddPullDownButton(shading_pulldown, "Heat Map", Scene::HEAT_MAP,
                            g_displayStyle==Scene::HEAT_MAP);
    g_hud.AddPullDownButton(shading_pulldown, "Ambient Occlusion", Scene::AO,
                            g_displayStyle==Scene::AO);
    g_hud.AddPullDownButton(shading_pulldown, "PBS", Scene::PBS,
                            g_displayStyle==Scene::PBS);

    for (int i = 1; i < 11; ++i) {
        char level[16];
        sprintf(level, "Lv. %d", i);
        g_hud.AddRadioButton(3, level, i==g_level, 10, y+i*20, callbackLevel, i, '0'+(i%10));
    }

    int pulldown_handle = g_hud.AddPullDown("Shape (N)", -300, 10, 300, callbackModel, 'n');
    for (int i = 0; i < (int)g_defaultShapes.size(); ++i) {
        g_hud.AddPullDownButton(pulldown_handle, g_defaultShapes[i].name.c_str(), i,
                                i == g_currentShape);
    }   
}

//------------------------------------------------------------------------------

static GLuint
compileProgram(const char *vs, const char *gs, const char *fs)
{
    GLuint vertexShader = GLUtils::CompileShader(GL_VERTEX_SHADER, vs);
    GLuint geometryShader = gs ? GLUtils::CompileShader(GL_GEOMETRY_SHADER, gs) : 0;
    GLuint fragmentShader = GLUtils::CompileShader(GL_FRAGMENT_SHADER, fs);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    if (geometryShader) glAttachShader(program, geometryShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (geometryShader) glDeleteShader(geometryShader);
    return program;
}

static void
initGL()
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glGenVertexArrays(1, &g_vao);
    glGenVertexArrays(1, &g_vaoBVH);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    static float pos[] = { -1, -1,
                            1, -1,
                           -1, 1,
                            1, 1,
                            0.5, -1,
                            1, -1,
                            0.5, -0.5,
                            1, -0.5 };
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_programBlockFill = compileProgram(s_VS, NULL, s_FS);
    g_programSimpleFill = compileProgram(s_VS, NULL, s0_FS);
    g_programBVH = compileProgram(s_VS_BVH, s_GS_BVH, s_FS_BVH);
    g_programDebug = compileProgram(s_VS_Debug, NULL, s_FS);
}

//------------------------------------------------------------------------------
static void
idle() {

    if (g_stepIndex == 0) {
        if (g_animate) {
            ++g_frame;
            updateGeom();
        }
        return;
    }

    g_renderTime = -1.0f;

    if (g_blockFill) {
        int index = g_step*g_step - g_stepIndex;
        index = ((index>>0)&1) * (g_step*g_step>>1)
            + ((index>>1)&1) * (g_step>>1)
            + ((index>>2)&1) * (g_step*g_step>>2)
            + ((index>>3)&1) * (g_step>>2)
            + ((index>>4)&1) * (g_step*g_step>>3)
            + ((index>>5)&1) * (g_step>>3);

        g_scene.Render(index, g_step);
        --g_stepIndex;
    } else {
        g_scene.Render();
        g_stepIndex = 0;
    }

    //if (g_displayStyle == Scene::PBS || g_displayStyle == Scene::AO) {
    if (g_displayStyle == Scene::PBS || g_displayStyle == Scene::AO) {
        if (g_stepIndex == 0) {
            g_stepIndex = g_step*g_step;
        }
    } else {
        if (g_stepIndex == 0) {
            g_renderTimer.Stop();
            g_renderTime = g_renderTimer.GetElapsed();
        }
    }

    display();
}

//------------------------------------------------------------------------------
// static void
// callbackError(OpenSubdiv::Osd::ErrorType err, const char *message)
// {
//     printf("OsdError: %d\n", err);
//     printf("%s", message);
// }

//------------------------------------------------------------------------------
static void
setGLCoreProfile()
{
    #define glfwOpenWindowHint glfwWindowHint
    #define GLFW_OPENGL_VERSION_MAJOR GLFW_CONTEXT_VERSION_MAJOR
    #define GLFW_OPENGL_VERSION_MINOR GLFW_CONTEXT_VERSION_MINOR

    glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if not defined(__APPLE__)
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 4);
#ifdef OPENSUBDIV_HAS_GLSL_COMPUTE
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
#else
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
#endif
    
#else
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
#endif
    glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
}

//--------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    std::string str;
    std::string envmapFile;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-d"))
            g_level = atoi(argv[++i]);
        else if(!strcmp(argv[i], "-H")) {
            g_intersectKernel = 1;
        } else if(!strcmp(argv[i], "-e")) {
            envmapFile = argv[++i];
        } else {
            std::ifstream ifs(argv[i]);
            if (ifs) {
                std::stringstream ss;
                ss << ifs.rdbuf();
                ifs.close();
                str = ss.str();
                g_defaultShapes.push_back(ShapeDesc(argv[i], str.c_str(), kCatmark));
            }
        }
    }
    initShapes();
    //OpenSubdiv::Osd::SetErrorCallback(callbackError);

    if (not glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return 1;
    }

    static const char windowTitle[] = "OpenSubdiv direct ray-tracing";

#define CORE_PROFILE
#ifdef CORE_PROFILE
    setGLCoreProfile();
#endif

    if (not (g_window=glfwCreateWindow(g_width, g_height, windowTitle, NULL, NULL))) {
        printf("Failed to open window.\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(g_window);

    glfwGetFramebufferSize(g_window, &g_frameBufferWidth, &g_frameBufferHeight);
    glfwSetFramebufferSizeCallback(g_window, reshape);

    glfwSetCharCallback(g_window, keyboardChar);
    glfwSetKeyCallback(g_window, keyboard);
    glfwSetCursorPosCallback(g_window, motion);
    glfwSetMouseButtonCallback(g_window, mouse);
    glfwSetWindowCloseCallback(g_window, windowClose);


#if defined(OSD_USES_GLEW)
#ifdef CORE_PROFILE
    // this is the only way to initialize glew correctly under core profile context.
    glewExperimental = true;
#endif
    if (GLenum r = glewInit() != GLEW_OK) {
        printf("Failed to initialize glew. Error = %s\n", glewGetErrorString(r));
        exit(1);
    }
#ifdef CORE_PROFILE
    // clear GL errors which was generated during glewInit()
    glGetError();
#endif
#endif

    //loadCamera();
    if (envmapFile.size()) {
        g_scene.LoadEnvMap(envmapFile);
        g_backgroundType = Scene::ENVMAP;
    }

    initGL();

    glfwSwapInterval(0);

    initHUD();
    rebuildOsdMesh();

    g_image.resize(g_width*g_height*4);
    g_scene.SetShadeMode((Scene::ShadeMode)g_displayStyle);

    while (g_running) {
        idle();

        glfwPollEvents();

        if (not g_animate and g_stepIndex == 0) glfwWaitEvents();
    }

    uninitGL();
    glfwTerminate();
}

//------------------------------------------------------------------------------
