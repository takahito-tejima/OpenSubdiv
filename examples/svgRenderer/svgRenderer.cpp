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

#include "../common/glUtils.h"

#include <GLFW/glfw3.h>
GLFWwindow* g_window=0;
GLFWmonitor* g_primary=0;

#include <far/error.h>

#include <far/stencilTableFactory.h>
#include <far/patchTableFactory.h>
#include <osd/cpuEvaluator.h>
#include <osd/cpuGLVertexBuffer.h>
#include <osd/glPatchTable.h>

#include "../../regression/common/far_utils.h"
#include "../common/glHud.h"
#include "../common/glUtils.h"
#include "../common/glShaderCache.h"
#include "../common/simple_math.h"
#include <osd/glslPatchShaderSource.h>

OpenSubdiv::Far::StencilTable const *g_stencilTable = NULL;
OpenSubdiv::Far::PatchTable const *g_patchTable = NULL;
OpenSubdiv::Osd::GLPatchTable const *g_glPatchTable = NULL;
OpenSubdiv::Osd::CpuGLVertexBuffer *g_vertexBuffer = NULL;


/* Function to get the correct shader file based on the opengl version.
  The implentation varies depending if glew is available or not. In case
  is available the capabilities are queried during execution and the correct
  source is returned. If glew in not available during compile time the version
  is determined*/
static const char *shaderSource(){
static const char *res =
#include "shader.gen.h"
;
return res;
}

#include <cfloat>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

enum DisplayStyle { kDisplayStyleWire,
                    kDisplayStyleShaded,
                    kDisplayStyleWireOnShaded };

enum HudCheckBox { kHUD_CB_ADAPTIVE };

int g_currentShape = 0;

bool g_axis=true;

// GUI variables
int   g_displayStyle = kDisplayStyleWireOnShaded,
      g_adaptive = 1,
      g_mbutton[3] = {0, 0, 0},
      g_running = 1;

float g_rotate[2] = {0, 0},
      g_dolly = 5,
      g_pan[2] = {0, 0},
      g_center[3] = {0, 0, 0},
      g_size = 0;

int   g_prev_x = 0,
      g_prev_y = 0;

int   g_width = 1024,
      g_height = 1024;

GLhud g_hud;

// geometry
std::vector<float> g_orgPositions;

int g_level = 2;
int g_tessLevel = 1;
int g_tessLevelMin = 1;
float g_moveScale = 0.0f;

GLuint g_transformUB = 0,
       g_transformBinding = 0,
       g_tessellationUB = 0,
       g_tessellationBinding = 1,
       g_lightingUB = 0,
       g_lightingBinding = 2;

struct Transform {
    float ModelViewMatrix[16];
    float ProjectionMatrix[16];
    float ModelViewProjectionMatrix[16];
    float ModelViewInverseMatrix[16];
} g_transformData;

GLuint g_vao = 0;

//------------------------------------------------------------------------------

#include "init_shapes.h"

//------------------------------------------------------------------------------
static void
updateGeom() {

    std::vector<float> vertex;

    int nverts = 0;
    int stride = 3;

    nverts = (int)g_orgPositions.size() / 3;

    vertex.reserve(nverts*stride);

    const float *p = &g_orgPositions[0];
    for (int i = 0; i < nverts; ++i) {
        vertex.push_back(p[0]);
        vertex.push_back(p[1]);
        vertex.push_back(p[2]);
        p += 3;
    }

    g_vertexBuffer->UpdateData(&vertex[0], 0, nverts);

    OpenSubdiv::Osd::BufferDescriptor srcDesc(0, 3, 3);
    OpenSubdiv::Osd::BufferDescriptor dstDesc(nverts*3, 3, 3);

    if (g_stencilTable->GetNumStencils() > 0) {
    OpenSubdiv::Osd::CpuEvaluator::EvalStencils(
        g_vertexBuffer, srcDesc, g_vertexBuffer, dstDesc,
        g_stencilTable);
    }
}

//------------------------------------------------------------------------------
static void
rebuildMesh() {
    using namespace OpenSubdiv;

    ShapeDesc const &shapeDesc = g_defaultShapes[g_currentShape];
    int level = g_level;
    Scheme scheme = shapeDesc.scheme;

    Shape const * shape = 0;
    shape = Shape::parseObj(shapeDesc.data.c_str(), shapeDesc.scheme,
                            shapeDesc.isLeftHanded);

    // create Far mesh (topology)
    Sdc::SchemeType sdctype = GetSdcType(*shape);
    Sdc::Options sdcoptions = GetSdcOptions(*shape);

    Far::TopologyRefiner * refiner =
        Far::TopologyRefinerFactory<Shape>::Create(*shape,
            Far::TopologyRefinerFactory<Shape>::Options(sdctype, sdcoptions));

    g_orgPositions = shape->verts;

    delete g_stencilTable;
    delete g_patchTable;
    delete g_glPatchTable;
    delete g_vertexBuffer;

    Far::TopologyRefiner::AdaptiveOptions options(level);
    refiner->RefineAdaptive(options);

    {
        Far::StencilTableFactory::Options options;
        options.generateOffsets = true;
        options.generateIntermediateLevels = true;
        g_stencilTable = Far::StencilTableFactory::Create(*refiner, options);
    }

    {
        Far::PatchTableFactory::Options poptions(level);
        poptions.SetEndCapType(Far::PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS);
        g_patchTable = Far::PatchTableFactory::Create(*refiner, poptions);
    }

    if (Far::StencilTable const *vertexStencilsWithLocalPoints =
        Far::StencilTableFactory::AppendLocalPointStencilTable(
            *refiner, g_stencilTable, g_patchTable->GetLocalPointStencilTable())) {
        delete g_stencilTable;
        g_stencilTable = vertexStencilsWithLocalPoints;
    }

    g_glPatchTable = Osd::GLPatchTable::Create(g_patchTable);
    g_vertexBuffer = Osd::CpuGLVertexBuffer::Create(3, g_stencilTable->GetNumControlVertices() + g_stencilTable->GetNumStencils());

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

    g_tessLevelMin = 1;

    g_tessLevel = std::max(g_tessLevel,g_tessLevelMin);

    updateGeom();

    // -------- VAO
    glBindVertexArray(g_vao);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_glPatchTable->GetPatchIndexBuffer());
    glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer->BindVBO());

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof (GLfloat) * 3, 0);
    glDisableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//------------------------------------------------------------------------------
static void
fitFrame() {

    g_pan[0] = g_pan[1] = 0;
    g_dolly = g_size;
}

//------------------------------------------------------------------------------

union Effect {
    Effect(int displayStyle_)
        : value(0) {
        displayStyle = displayStyle_;
    }

    struct {
        unsigned int displayStyle:2;
    };
    int value;

    bool operator < (const Effect &e) const {
        return value < e.value;
    }
};

static Effect
GetEffect()
{
    return Effect(g_displayStyle);
}

// ---------------------------------------------------------------------------

struct EffectDesc {
    EffectDesc(OpenSubdiv::Far::PatchDescriptor desc,
               Effect effect) : desc(desc), effect(effect),
                                maxValence(0), numElements(0) { }

    OpenSubdiv::Far::PatchDescriptor desc;
    Effect effect;
    int maxValence;
    int numElements;

    bool operator < (const EffectDesc &e) const {
        return
            (desc < e.desc || ((desc == e.desc &&
            (maxValence < e.maxValence || ((maxValence == e.maxValence) &&
            (numElements < e.numElements || ((numElements == e.numElements) &&
            (effect < e.effect))))))));
    }
};

// ---------------------------------------------------------------------------

class ShaderCache : public GLShaderCache<EffectDesc> {
public:
    virtual GLDrawConfig *CreateDrawConfig(EffectDesc const &effectDesc) {

        using namespace OpenSubdiv;

        // compile shader program

        GLDrawConfig *config = new GLDrawConfig(GLUtils::GetShaderVersionInclude().c_str());

        Far::PatchDescriptor::Type type = effectDesc.desc.GetType();

        // common defines
        std::stringstream ss;

        if (type == Far::PatchDescriptor::QUADS) {
            ss << "#define PRIM_QUAD\n";
        } else {
            ss << "#define PRIM_TRI\n";
        }

        // OSD tessellation controls
//        ss << "#define OSD_PATCH_ENABLE_SINGLE_CREASE\n";

        // for legacy gregory
        ss << "#define OSD_MAX_VALENCE " << effectDesc.maxValence << "\n";
        ss << "#define OSD_NUM_ELEMENTS " << effectDesc.numElements << "\n";

        // display styles
        switch (effectDesc.effect.displayStyle) {
        case kDisplayStyleWire:
            ss << "#define GEOMETRY_OUT_WIRE\n";
            break;
        case kDisplayStyleWireOnShaded:
            ss << "#define GEOMETRY_OUT_LINE\n";
            break;
        case kDisplayStyleShaded:
            ss << "#define GEOMETRY_OUT_FILL\n";
            break;
        }

        if (type == Far::PatchDescriptor::TRIANGLES) {
            ss << "#define LOOP\n";
        } else if (type == Far::PatchDescriptor::QUADS) {
        } else {
            ss << "#define SMOOTH_NORMALS\n";
        }

        // include osd PatchCommon
        ss << Osd::GLSLPatchShaderSource::GetCommonShaderSource();
        std::string common = ss.str();
        ss.str("");

        // vertex shader
        ss << common
            // enable local vertex shader
           << (effectDesc.desc.IsAdaptive() ? "" : "#define VERTEX_SHADER\n")
           << shaderSource()
           << Osd::GLSLPatchShaderSource::GetVertexShaderSource(type);
        config->CompileAndAttachShader(GL_VERTEX_SHADER, ss.str());
        ss.str("");

        if (effectDesc.desc.IsAdaptive()) {
            // tess control shader
            ss << common
                << shaderSource()
               << Osd::GLSLPatchShaderSource::GetTessControlShaderSource(type);
            config->CompileAndAttachShader(GL_TESS_CONTROL_SHADER, ss.str());
            ss.str("");

            // tess eval shader
            ss << common
                << shaderSource()
               << Osd::GLSLPatchShaderSource::GetTessEvalShaderSource(type);
            config->CompileAndAttachShader(GL_TESS_EVALUATION_SHADER, ss.str());
            ss.str("");
        }

        // geometry shader
        ss << common
           << "#define GEOMETRY_SHADER\n"
           << shaderSource();
        config->CompileAndAttachShader(GL_GEOMETRY_SHADER, ss.str());
        ss.str("");

        // fragment shader
        ss << common
           << "#define FRAGMENT_SHADER\n"
           << shaderSource();
        config->CompileAndAttachShader(GL_FRAGMENT_SHADER, ss.str());
        ss.str("");

        if (!config->Link()) {
            delete config;
            return NULL;
        }

        // assign uniform locations
        GLuint uboIndex;
        GLuint program = config->GetProgram();
        uboIndex = glGetUniformBlockIndex(program, "Transform");
        if (uboIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(program, uboIndex, g_transformBinding);

        uboIndex = glGetUniformBlockIndex(program, "Tessellation");
        if (uboIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(program, uboIndex, g_tessellationBinding);

        uboIndex = glGetUniformBlockIndex(program, "Lighting");
        if (uboIndex != GL_INVALID_INDEX)
            glUniformBlockBinding(program, uboIndex, g_lightingBinding);

        // assign texture locations
        GLint loc;
        glUseProgram(program);
        if ((loc = glGetUniformLocation(program, "OsdPatchParamBuffer")) != -1) {
            glUniform1i(loc, 0); // GL_TEXTURE0
        }
        glUseProgram(0);

        return config;
    }
};

ShaderCache g_shaderCache;

//------------------------------------------------------------------------------
static void
updateUniformBlocks() {
    if (! g_transformUB) {
        glGenBuffers(1, &g_transformUB);
        glBindBuffer(GL_UNIFORM_BUFFER, g_transformUB);
        glBufferData(GL_UNIFORM_BUFFER,
                sizeof(g_transformData), NULL, GL_STATIC_DRAW);
    };
    glBindBuffer(GL_UNIFORM_BUFFER, g_transformUB);
    glBufferSubData(GL_UNIFORM_BUFFER,
                0, sizeof(g_transformData), &g_transformData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, g_transformBinding, g_transformUB);

    // Update and bind tessellation state
    struct Tessellation {
        float TessLevel;
    } tessellationData;

    tessellationData.TessLevel = static_cast<float>(1 << g_tessLevel);

    if (! g_tessellationUB) {
        glGenBuffers(1, &g_tessellationUB);
        glBindBuffer(GL_UNIFORM_BUFFER, g_tessellationUB);
        glBufferData(GL_UNIFORM_BUFFER,
                sizeof(tessellationData), NULL, GL_STATIC_DRAW);
    };
    glBindBuffer(GL_UNIFORM_BUFFER, g_tessellationUB);
    glBufferSubData(GL_UNIFORM_BUFFER,
                0, sizeof(tessellationData), &tessellationData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, g_tessellationBinding, g_tessellationUB);

    // Update and bind lighting state
    struct Lighting {
        struct Light {
            float position[4];
            float ambient[4];
            float diffuse[4];
            float specular[4];
        } lightSource[2];
    } lightingData = {
       {{  { 0.5,  0.2f, 1.0f, 0.0f },
           { 0.1f, 0.1f, 0.1f, 1.0f },
           { 0.7f, 0.7f, 0.7f, 1.0f },
           { 0.8f, 0.8f, 0.8f, 1.0f } },

         { { -0.8f, 0.4f, -1.0f, 0.0f },
           {  0.0f, 0.0f,  0.0f, 1.0f },
           {  0.5f, 0.5f,  0.5f, 1.0f },
           {  0.8f, 0.8f,  0.8f, 1.0f } }}
    };
    if (! g_lightingUB) {
        glGenBuffers(1, &g_lightingUB);
        glBindBuffer(GL_UNIFORM_BUFFER, g_lightingUB);
        glBufferData(GL_UNIFORM_BUFFER,
                sizeof(lightingData), NULL, GL_STATIC_DRAW);
    };
    glBindBuffer(GL_UNIFORM_BUFFER, g_lightingUB);
    glBufferSubData(GL_UNIFORM_BUFFER,
                0, sizeof(lightingData), &lightingData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, g_lightingBinding, g_lightingUB);
}

static void
bindTextures() {
    // bind patch textures
    if (g_glPatchTable->GetPatchParamTextureBuffer()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER,
            g_glPatchTable->GetPatchParamTextureBuffer());
    }
    glActiveTexture(GL_TEXTURE0);
}

static GLenum
bindProgram(Effect effect,
            OpenSubdiv::Osd::PatchArray const & patch) {
    EffectDesc effectDesc(patch.GetDescriptor(), effect);

    // lookup shader cache (compile the shader if needed)
    GLDrawConfig *config = g_shaderCache.GetDrawConfig(effectDesc);
    if (!config) return 0;

    GLuint program = config->GetProgram();

    glUseProgram(program);

    // bind standalone uniforms
    GLint uniformPrimitiveIdBase =
        glGetUniformLocation(program, "PrimitiveIdBase");
    if (uniformPrimitiveIdBase >=0)
        glUniform1i(uniformPrimitiveIdBase, patch.GetPrimitiveIdBase());

    // update uniform
    GLint uniformDiffuseColor =
        glGetUniformLocation(program, "diffuseColor");
    if (uniformDiffuseColor >= 0)
        glUniform4f(uniformDiffuseColor, 0.4f, 0.4f, 0.8f, 1);

    // return primtype
    GLenum primType;
    switch(effectDesc.desc.GetType()) {
    case OpenSubdiv::Far::PatchDescriptor::QUADS:
        primType = GL_LINES_ADJACENCY;
        break;
    case OpenSubdiv::Far::PatchDescriptor::TRIANGLES:
        primType = GL_TRIANGLES;
        break;
    default:
#if defined(GL_ARB_tessellation_shader) || defined(GL_VERSION_4_0)
        primType = GL_PATCHES;
        glPatchParameteri(GL_PATCH_VERTICES, effectDesc.desc.GetNumControlVertices());
#else
        primType = GL_POINTS;
#endif
        break;
    }

    return primType;
}

//------------------------------------------------------------------------------
static void
display() {
    updateGeom();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, g_width, g_height);
    g_hud.FillBackground();

    // prepare view matrix
    double aspect = g_width/(double)g_height;
    identity(g_transformData.ModelViewMatrix);
    translate(g_transformData.ModelViewMatrix, -g_pan[0], -g_pan[1], -g_dolly);
    rotate(g_transformData.ModelViewMatrix, g_rotate[1], 1, 0, 0);
    rotate(g_transformData.ModelViewMatrix, g_rotate[0], 0, 1, 0);
    rotate(g_transformData.ModelViewMatrix, -90, 1, 0, 0);
    translate(g_transformData.ModelViewMatrix,
              -g_center[0], -g_center[1], -g_center[2]);
    perspective(g_transformData.ProjectionMatrix,
                30.0f, (float)aspect, 0.1f, 500.0f);
    multMatrix(g_transformData.ModelViewProjectionMatrix,
               g_transformData.ModelViewMatrix,
               g_transformData.ProjectionMatrix);
    inverseMatrix(g_transformData.ModelViewInverseMatrix,
                  g_transformData.ModelViewMatrix);

    // make sure that the vertex buffer is interoped back as a GL resources.
    g_vertexBuffer->BindVBO();

    // update transform and lighting uniform blocks
    updateUniformBlocks();

    // also bind patch related textures
    bindTextures();

    if (g_displayStyle == kDisplayStyleWire)
        glDisable(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_glPatchTable->GetPatchIndexBuffer());
    glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer->BindVBO());

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof (GLfloat) * 3, 0);


    OpenSubdiv::Osd::PatchArrayVector const & patches =
        g_glPatchTable->GetPatchArrays();

    // core draw-calls
    for (int i=0; i<(int)patches.size(); ++i) {
        OpenSubdiv::Osd::PatchArray const & patch = patches[i];

        OpenSubdiv::Far::PatchDescriptor desc = patch.GetDescriptor();

        GLenum primType = bindProgram(GetEffect(), patch);

        glDrawElements(primType,
                       patch.GetNumPatches() * desc.GetNumControlVertices(),
                       GL_UNSIGNED_INT,
                       (void *)(patch.GetIndexBase() * sizeof(unsigned int)));
    }

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glUseProgram(0);

    if (g_displayStyle == kDisplayStyleWire)
        glEnable(GL_CULL_FACE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (g_hud.IsVisible()) {
        int y = -220;
        g_hud.DrawString(10, y, "Tess level : %d", g_tessLevel); y+= 20;
        g_hud.Flush();
    }

    glFinish();

    GLUtils::CheckGLErrors("display leave\n");
}

//------------------------------------------------------------------------------
static void
motion(GLFWwindow *, double dx, double dy) {

    int x=(int)dx, y=(int)dy;

    if (g_hud.MouseCapture()) {
        // check gui
        g_hud.MouseMotion(x, y);
    } else if (g_mbutton[0] && !g_mbutton[1] && !g_mbutton[2]) {
        // orbit
        g_rotate[0] += x - g_prev_x;
        g_rotate[1] += y - g_prev_y;
    } else if (!g_mbutton[0] && !g_mbutton[1] && g_mbutton[2]) {
        // pan
        g_pan[0] -= g_dolly*(x - g_prev_x)/g_width;
        g_pan[1] += g_dolly*(y - g_prev_y)/g_height;
    } else if ((g_mbutton[0] && !g_mbutton[1] && g_mbutton[2]) or
               (!g_mbutton[0] && g_mbutton[1] && !g_mbutton[2])) {
        // dolly
        g_dolly -= g_dolly*0.01f*(x - g_prev_x);
        if(g_dolly <= 0.01) g_dolly = 0.01f;
    }

    g_prev_x = x;
    g_prev_y = y;
}

//------------------------------------------------------------------------------
static void
mouse(GLFWwindow *, int button, int state, int /* mods */) {

    if (state == GLFW_RELEASE)
        g_hud.MouseRelease();

    if (button == 0 && state == GLFW_PRESS && g_hud.MouseClick(g_prev_x, g_prev_y))
        return;

    if (button < 3) {
        g_mbutton[button] = (state == GLFW_PRESS);
    }
}

//------------------------------------------------------------------------------
static void
uninitGL() {

    glDeleteVertexArrays(1, &g_vao);
}

//------------------------------------------------------------------------------
static void
reshape(GLFWwindow *, int width, int height) {

    g_width = width;
    g_height = height;

    int windowWidth = g_width, windowHeight = g_height;

    // window size might not match framebuffer size on a high DPI display
    glfwGetWindowSize(g_window, &windowWidth, &windowHeight);

    g_hud.Rebuild(windowWidth, windowHeight, width, height);
}

//------------------------------------------------------------------------------
void windowClose(GLFWwindow*) {
    g_running = false;
}

static void
writeSVG() {

    using namespace OpenSubdiv;

    Osd::PatchArrayVector const & patches =
        g_glPatchTable->GetPatchArrays();

    FILE *fp = fopen("out.html", "w");

    fprintf(fp, "<html>\n");

    float scale = 1;
    float viewport = 256;

    fprintf(fp, "<svg height='%f' width='%f'>\n", viewport, viewport);
    fprintf(fp, "<defs><style type='text/css'>\n");
    fprintf(fp, "<![CDATA[\n");
    fprintf(fp, "  path { stroke: #1f83c1; stroke-width: 1; fill: white; }\n");
    fprintf(fp, "]]>\n");
    fprintf(fp, "</style></defs>\n");

    // transform points
    std::vector<float> pos(g_vertexBuffer->GetNumVertices() * 3);
    float *sp = g_vertexBuffer->BindCpuBuffer();
    for (int i = 0; i < g_vertexBuffer->GetNumVertices(); ++i) {
        float op[4];
        op[0] = sp[i*3+0];
        op[1] = sp[i*3+1];
        op[2] = sp[i*3+2];
        op[3] = 1.0;
        apply(op, g_transformData.ModelViewMatrix);

        pos[i*3+0] = op[0];
        pos[i*3+1] = op[1];
        pos[i*3+2] = op[2];
    }

    // core draw-calls
    std::vector<std::pair<float, std::string> > zSort;

    for (int array=0; array<(int)patches.size(); ++array) {
        Osd::PatchArray const & patch = patches[array];
        Far::PatchDescriptor desc = patch.GetDescriptor();

        if (desc.GetType() != Far::PatchDescriptor::REGULAR) continue;

        int numPatches = patch.GetNumPatches();
        std::stringstream ss;

        for (int patch = 0 ; patch < numPatches; ++patch) {

            Far::ConstIndexArray indices = g_patchTable->GetPatchVertices(array, patch);
            if (indices.size() != 16) continue;

            // convert to bezier
            float Q[16] = {1.f/6.f, 4.f/6.f, 1.f/6.f, 0.f,
                           0.f,     4.f/6.f, 2.f/6.f, 0.f,
                           0.f,     2.f/6.f, 4.f/6.f, 0.f,
                           0.f,     1.f/6.f, 4.f/6.f, 1.f/6.f};

            // TODO: boundary, single-crease

            float bp[16][3];
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    float H[4][3];
                    for (int l = 0; l < 4; ++l) {
                        H[l][0] = H[l][1] = H[l][2] = 0;
                        for (int k = 0; k < 4; ++k) {
                            H[l][0] += Q[i*4+k] * pos[indices[l*4 + k]*3+0];;
                            H[l][1] += Q[i*4+k] * pos[indices[l*4 + k]*3+1];;
                            H[l][2] += Q[i*4+k] * pos[indices[l*4 + k]*3+2];;
                        }
                    }
                    bp[i*4+j][0] = bp[i*4+j][1] = bp[i*4+j][2] = 0;
                    for (int k=0; k<4; ++k) {
                        bp[i*4+j][0] += Q[j*4+k] * H[k][0];
                        bp[i*4+j][1] += Q[j*4+k] * H[k][1];
                        bp[i*4+j][2] += Q[j*4+k] * H[k][2];
                    }
                }
            }

            // projection(?) + viewport
            if (true) {
                for (int i = 0; i < 16; ++i) {
                    float op[4];
                    op[0] = bp[i][0];
                    op[1] = bp[i][1];
                    op[2] = bp[i][2];
                    op[3] = 1;
                    apply(op, g_transformData.ProjectionMatrix);
                    bp[i][0] = (op[0]/op[3] + 1.0) * viewport * 0.5;
                    bp[i][1] = (2 - op[1]/op[3] - 1.0) * viewport * 0.5;
                    bp[i][2] = -op[2]/op[3];
                }
            }
            ss.str("");

            ss << "<path id='patch" << patch << "' d='";
            ss << "M " << bp[0][0] << " " << bp[0][1]
               << "C " << bp[1][0] << " " << bp[1][1]
               << "  " << bp[2][0] << " " << bp[2][1]
               << "  " << bp[3][0] << " " << bp[3][1]
               << "  " << bp[7][0] << " " << bp[7][1]
               << "  " << bp[11][0] << " " << bp[11][1]
               << "  " << bp[15][0] << " " << bp[15][1]
               << "  " << bp[14][0] << " " << bp[14][1]
               << "  " << bp[13][0] << " " << bp[13][1]
               << "  " << bp[12][0] << " " << bp[12][1]
               << "  " << bp[8][0] << " " << bp[8][1]
               << "  " << bp[4][0] << " " << bp[4][1]
               << "  " << bp[0][0] << " " << bp[0][1];
            ss << "'/>\n";

            zSort.push_back(std::make_pair(bp[5][2], ss.str()));
        }
    }
    std::sort(zSort.begin(), zSort.end());
    for (int i = 0; i < zSort.size(); ++i) {
        fprintf(fp, "%s\n", zSort[i].second.c_str());
    }

    fprintf(fp, "</svg>\n");
    fprintf(fp, "</html>\n");
    fclose(fp);
}

//------------------------------------------------------------------------------
static void
keyboard(GLFWwindow *, int key, int /* scancode */, int event, int /* mods */) {

    if (event == GLFW_RELEASE) return;
    if (g_hud.KeyDown(tolower(key))) return;

    switch (key) {
        case 'Q': g_running = 0; break;
        case 'F': fitFrame(); break;
        case '+':
        case '=':  g_tessLevel++; break;
        case '-':  g_tessLevel = std::max(g_tessLevelMin, g_tessLevel-1); break;
        case GLFW_KEY_ESCAPE: g_hud.SetVisible(!g_hud.IsVisible()); break;
        case 'X': GLUtils::WriteScreenshot(g_width, g_height); break;
        case ' ': writeSVG(); break;
    }
}

//------------------------------------------------------------------------------
static void
callbackDisplayStyle(int b) {
    g_displayStyle = b;
}

static void
callbackLevel(int l) {
    g_level = l;
    rebuildMesh();
}

static void
callbackModel(int m) {
    if (m < 0)
        m = 0;

    if (m >= (int)g_defaultShapes.size())
        m = (int)g_defaultShapes.size() - 1;

    g_currentShape = m;
    rebuildMesh();
}

static void
callbackCheckBox(bool checked, int button) {

    if (GLUtils::SupportsAdaptiveTessellation()) {
        switch(button) {
        case kHUD_CB_ADAPTIVE:
            g_adaptive = checked;
            rebuildMesh();
            return;
        default:
            break;
        }
    }
}

static void
initHUD() {
    int windowWidth = g_width, windowHeight = g_height;
    int frameBufferWidth = g_width, frameBufferHeight = g_height;

    // window size might not match framebuffer size on a high DPI display
    glfwGetWindowSize(g_window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(g_window, &frameBufferWidth, &frameBufferHeight);

    g_hud.Init(windowWidth, windowHeight, frameBufferWidth, frameBufferHeight);

    int displaystyle_pulldown = g_hud.AddPullDown("DisplayStyle (W)", 200, 10, 250,
                                                  callbackDisplayStyle, 'w');
    g_hud.AddPullDownButton(displaystyle_pulldown, "Wire", kDisplayStyleWire,
                            g_displayStyle == kDisplayStyleWire);
    g_hud.AddPullDownButton(displaystyle_pulldown, "Shaded", kDisplayStyleShaded,
                            g_displayStyle == kDisplayStyleShaded);
    g_hud.AddPullDownButton(displaystyle_pulldown, "Wire+Shaded", kDisplayStyleWireOnShaded,
                            g_displayStyle == kDisplayStyleWireOnShaded);

    if (GLUtils::SupportsAdaptiveTessellation()) {
        g_hud.AddCheckBox("Adaptive (`)", g_adaptive!=0,
                          10, 190, callbackCheckBox, kHUD_CB_ADAPTIVE, '`');
    }

    for (int i = 1; i < 11; ++i) {
        char level[16];
        sprintf(level, "Lv. %d", i);
        g_hud.AddRadioButton(3, level, i==2, 10, 310+i*20, callbackLevel, i, '0'+(i%10));
    }

    int shapes_pulldown = g_hud.AddPullDown("Shape (N)", -300, 10, 300, callbackModel, 'n');
    for (int i = 0; i < (int)g_defaultShapes.size(); ++i) {
        g_hud.AddPullDownButton(shapes_pulldown, g_defaultShapes[i].name.c_str(),i);
    }

    g_hud.Rebuild(windowWidth, windowHeight, frameBufferWidth, frameBufferHeight);
}

//------------------------------------------------------------------------------
static void
initGL() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glGenVertexArrays(1, &g_vao);
}

//------------------------------------------------------------------------------
static void
callbackErrorOsd(OpenSubdiv::Far::ErrorType err, const char *message) {
    printf("Error: %d\n", err);
    printf("%s", message);
}

//------------------------------------------------------------------------------
static void
callbackErrorGLFW(int error, const char* description) {
    fprintf(stderr, "GLFW Error (%d) : %s\n", error, description);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main(int argc, char ** argv) {

    std::string str;
    std::vector<char const *> animobjs;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-axis")) {
            g_axis = false;
        }
        else if (!strcmp(argv[i], "-d")) {
            g_level = atoi(argv[++i]);
        }
        else {
            std::ifstream ifs(argv[1]);
            if (ifs) {
                std::stringstream ss;
                ss << ifs.rdbuf();
                ifs.close();
                str = ss.str();
                g_defaultShapes.push_back(ShapeDesc(argv[1], str.c_str(), kCatmark));
            }
        }
    }

    initShapes();

    OpenSubdiv::Far::SetErrorCallback(callbackErrorOsd);

    glfwSetErrorCallback(callbackErrorGLFW);
    if (not glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return 1;
    }

    static const char windowTitle[] = "OpenSubdiv glViewer " OPENSUBDIV_VERSION_STRING;

    GLUtils::SetMinimumGLVersion(argc, argv);

    g_window = glfwCreateWindow(g_width, g_height, windowTitle, NULL, NULL);

    if (not g_window) {
        std::cerr << "Failed to create OpenGL context.\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(g_window);
    GLUtils::PrintGLVersion();

    // accommocate high DPI displays (e.g. mac retina displays)
    glfwGetFramebufferSize(g_window, &g_width, &g_height);
    glfwSetFramebufferSizeCallback(g_window, reshape);

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

    // activate feature adaptive tessellation if OSD supports it
    g_adaptive = false;// GLUtils::SupportsAdaptiveTessellation();

    initGL();

    glfwSwapInterval(0);

    initHUD();
    rebuildMesh();

    while (g_running) {
        display();

        glfwWaitEvents();
        glfwSwapBuffers(g_window);

        glFinish();
    }

    uninitGL();
    glfwTerminate();
}

//------------------------------------------------------------------------------
