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

#if defined(GLFW_VERSION_3)
    #include <GLFW/glfw3.h>
    GLFWwindow* g_window=0;
    GLFWmonitor* g_primary=0;
#else
    #include <GL/glfw.h>
#endif

#include <osd/error.h>
#include <osd/vertex.h>
#include <osd/glDrawContext.h>
#include <osd/glDrawRegistry.h>
#include <osd/glMesh.h>


#include <shapes/catmark_cube.h>

#include "../common/simple_math.h"
#include "../common/gl_hud.h"

static const char *shaderSource =
#include "shader.gen.h"
;

#include <vector>
#include <fstream>
#include <sstream>

#include "mesh.h"

// ---------------------------------------------------------------------------

enum DisplayStyle { kWire = 0,
                    kShaded,
                    kWireShaded };

enum HudCheckBox { kHUD_CB_FREEZE };

// GUI variables
int   g_displayStyle = kShaded,
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
Mesh *g_mesh = NULL;

int g_level = 2;
int g_tessLevel = 1;
int g_tessLevelMin = 1;

GLuint g_transformUB = 0,
       g_transformBinding = 0,
       g_tessellationUB = 0,
       g_tessellationBinding = 0,
       g_lightingUB = 0,
       g_lightingBinding = 0;

struct Transform {
    float ModelViewMatrix[16];
    float ProjectionMatrix[16];
    float ModelViewProjectionMatrix[16];
} g_transformData;

GLuint g_queries[2] = {0, 0};
GLuint g_vao = 0;

//------------------------------------------------------------------------------
static void
fitFrame() {

    g_pan[0] = g_pan[1] = 0;
    g_dolly = g_size;
}

//------------------------------------------------------------------------------

union Effect {
    Effect(int displayStyle_) : value(0) {
        displayStyle = displayStyle_;
    }

    struct {
        unsigned int displayStyle:3;
    };
    int value;

    bool operator < (const Effect &e) const {
        return value < e.value;
    }
};

typedef std::pair<OpenSubdiv::OsdDrawContext::PatchDescriptor, Effect> EffectDesc;

class EffectDrawRegistry : public OpenSubdiv::OsdGLDrawRegistry<EffectDesc> {

protected:
    virtual ConfigType *
    _CreateDrawConfig(DescType const & desc, SourceConfigType const * sconfig);

    virtual SourceConfigType *
    _CreateDrawSourceConfig(DescType const & desc);
};

EffectDrawRegistry::SourceConfigType *
EffectDrawRegistry::_CreateDrawSourceConfig(DescType const & desc)
{
    Effect effect = desc.second;

    SourceConfigType * sconfig =
        BaseRegistry::_CreateDrawSourceConfig(desc.first);

    assert(sconfig);

    const char *glslVersion = "#version 400\n";
    if (desc.first.GetType() == OpenSubdiv::FarPatchTables::QUADS or
        desc.first.GetType() == OpenSubdiv::FarPatchTables::TRIANGLES) {
        sconfig->vertexShader.source = shaderSource;
        sconfig->vertexShader.version = glslVersion;
        sconfig->vertexShader.AddDefine("VERTEX_SHADER");
    } else {
        sconfig->geometryShader.AddDefine("SMOOTH_NORMALS");
    }

    sconfig->geometryShader.source = shaderSource;
    sconfig->geometryShader.version = glslVersion;
    sconfig->geometryShader.AddDefine("GEOMETRY_SHADER");

    sconfig->fragmentShader.source = shaderSource;
    sconfig->fragmentShader.version = glslVersion;
    sconfig->fragmentShader.AddDefine("FRAGMENT_SHADER");

    if (desc.first.GetType() == OpenSubdiv::FarPatchTables::QUADS) {
        // uniform catmark, bilinear
        sconfig->geometryShader.AddDefine("PRIM_QUAD");
        sconfig->fragmentShader.AddDefine("PRIM_QUAD");
        sconfig->commonShader.AddDefine("UNIFORM_SUBDIVISION");
    } else if (desc.first.GetType() == OpenSubdiv::FarPatchTables::TRIANGLES) {
        // uniform loop
        sconfig->geometryShader.AddDefine("PRIM_TRI");
        sconfig->fragmentShader.AddDefine("PRIM_TRI");
        sconfig->commonShader.AddDefine("LOOP");
        sconfig->commonShader.AddDefine("UNIFORM_SUBDIVISION");
    } else {
        // adaptive
        sconfig->vertexShader.source = shaderSource + sconfig->vertexShader.source;
        sconfig->tessControlShader.source = shaderSource + sconfig->tessControlShader.source;
        sconfig->tessEvalShader.source = shaderSource + sconfig->tessEvalShader.source;

        sconfig->geometryShader.AddDefine("PRIM_TRI");
        sconfig->fragmentShader.AddDefine("PRIM_TRI");
    }

    switch (effect.displayStyle) {
    case kWire:
        sconfig->commonShader.AddDefine("GEOMETRY_OUT_WIRE");
        break;
    case kWireShaded:
        sconfig->commonShader.AddDefine("GEOMETRY_OUT_LINE");
        break;
    case kShaded:
        sconfig->commonShader.AddDefine("GEOMETRY_OUT_FILL");
        break;
    }

    return sconfig;
}

EffectDrawRegistry::ConfigType *
EffectDrawRegistry::_CreateDrawConfig(
        DescType const & desc,
        SourceConfigType const * sconfig)
{
    ConfigType * config = BaseRegistry::_CreateDrawConfig(desc.first, sconfig);
    assert(config);

    GLuint uboIndex;

    // XXXdyu can use layout(binding=) with GLSL 4.20 and beyond
    g_transformBinding = 0;
    uboIndex = glGetUniformBlockIndex(config->program, "Transform");
    if (uboIndex != GL_INVALID_INDEX)
        glUniformBlockBinding(config->program, uboIndex, g_transformBinding);

    g_tessellationBinding = 1;
    uboIndex = glGetUniformBlockIndex(config->program, "Tessellation");
    if (uboIndex != GL_INVALID_INDEX)
        glUniformBlockBinding(config->program, uboIndex, g_tessellationBinding);

    g_lightingBinding = 2;
    uboIndex = glGetUniformBlockIndex(config->program, "Lighting");
    if (uboIndex != GL_INVALID_INDEX)
        glUniformBlockBinding(config->program, uboIndex, g_lightingBinding);

    GLint loc;
    if ((loc = glGetUniformLocation(config->program, "OsdVertexBuffer")) != -1) {
        glProgramUniform1i(config->program, loc, 0); // GL_TEXTURE0
    }
    if ((loc = glGetUniformLocation(config->program, "OsdValenceBuffer")) != -1) {
        glProgramUniform1i(config->program, loc, 1); // GL_TEXTURE1
    }
    if ((loc = glGetUniformLocation(config->program, "OsdQuadOffsetBuffer")) != -1) {
        glProgramUniform1i(config->program, loc, 2); // GL_TEXTURE2
    }
    if ((loc = glGetUniformLocation(config->program, "OsdPatchParamBuffer")) != -1) {
        glProgramUniform1i(config->program, loc, 3); // GL_TEXTURE3
    }
    if ((loc = glGetUniformLocation(config->program, "OsdFVarDataBuffer")) != -1) {
        glProgramUniform1i(config->program, loc, 4); // GL_TEXTURE4
    }

    return config;
}

EffectDrawRegistry effectRegistry;

static Effect
GetEffect()
{
    return Effect(g_displayStyle);
}

//------------------------------------------------------------------------------
static GLuint
bindProgram(Effect effect, OpenSubdiv::OsdDrawContext::PatchArray const & patch)
{
    EffectDesc effectDesc(patch.GetDescriptor(), effect);
    EffectDrawRegistry::ConfigType *
        config = effectRegistry.GetDrawConfig(effectDesc);

    GLuint program = config->program;

    glUseProgram(program);

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

    OpenSubdiv::OsdGLDrawContext *drawContext = g_mesh->GetDrawContext();

    if (drawContext->GetVertexTextureBuffer()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER,
            drawContext->GetVertexTextureBuffer());
    }
    if (drawContext->GetVertexValenceTextureBuffer()) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER,
            drawContext->GetVertexValenceTextureBuffer());
    }
    if (drawContext->GetQuadOffsetsTextureBuffer()) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_BUFFER,
            drawContext->GetQuadOffsetsTextureBuffer());
    }
    if (drawContext->GetPatchParamTextureBuffer()) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_BUFFER,
            drawContext->GetPatchParamTextureBuffer());
    }
    if (drawContext->GetFvarDataTextureBuffer()) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_BUFFER,
            drawContext->GetFvarDataTextureBuffer());
    }

    glActiveTexture(GL_TEXTURE0);

    return program;
}

//------------------------------------------------------------------------------
static int
drawPatches(OpenSubdiv::OsdDrawContext::PatchArrayVector const &patches,
            GLfloat const *color)
{
    int numDrawCalls = 0;
    for (int i=0; i<(int)patches.size(); ++i) {
        OpenSubdiv::OsdDrawContext::PatchArray const & patch = patches[i];

        OpenSubdiv::OsdDrawContext::PatchDescriptor desc = patch.GetDescriptor();
        OpenSubdiv::FarPatchTables::Type patchType = desc.GetType();

        GLenum primType;

        switch(patchType) {
        case OpenSubdiv::FarPatchTables::QUADS:
            primType = GL_LINES_ADJACENCY;
            break;
        case OpenSubdiv::FarPatchTables::TRIANGLES:
            primType = GL_TRIANGLES;
            break;
        default:
            primType = GL_PATCHES;
            glPatchParameteri(GL_PATCH_VERTICES, desc.GetNumControlVertices());
        }

        GLuint program = bindProgram(GetEffect(), patch);

        GLuint uniformColor =
          glGetUniformLocation(program, "diffuseColor");
        glProgramUniform4f(program, uniformColor, color[0], color[1], color[2], 1);

        GLuint uniformGregoryQuadOffsetBase =
          glGetUniformLocation(program, "GregoryQuadOffsetBase");
        GLuint uniformPrimitiveIdBase =
          glGetUniformLocation(program, "PrimitiveIdBase");

        glProgramUniform1i(program, uniformGregoryQuadOffsetBase,
                           patch.GetQuadOffsetIndex());
        glProgramUniform1i(program, uniformPrimitiveIdBase,
                           patch.GetPatchIndex());

        GLvoid *indices = (void *)(patch.GetVertIndex() * sizeof(unsigned int));

        glDrawElements(primType,
                       patch.GetNumIndices(),
                       GL_UNSIGNED_INT,
                       indices);
        ++numDrawCalls;
    }
    return numDrawCalls;
}
static void
display() {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, g_width, g_height);

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
                45.0f, (float)aspect, 0.01f, 500.0f);
    multMatrix(g_transformData.ModelViewProjectionMatrix,
               g_transformData.ModelViewMatrix,
               g_transformData.ProjectionMatrix);

    // make sure that the vertex buffer is interoped back as a GL resources.
    g_mesh->BindVertexBuffer();

    glBindVertexArray(g_vao);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                 g_mesh->GetDrawContext()->GetPatchIndexBuffer());

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, g_mesh->BindVertexBuffer());
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof (GLfloat) * 3, 0);
    glDisableVertexAttribArray(1);

    OpenSubdiv::OsdDrawContext::PatchArrayVector const & patches =
        g_mesh->GetDrawContext()->patchArrays;
    int numDrawCalls = 0;
    // primitive counting
    glBeginQuery(GL_PRIMITIVES_GENERATED, g_queries[0]);
    glBeginQuery(GL_TIME_ELAPSED, g_queries[1]);

    // draw instances with same topology
    GLfloat color[3] = {1.0, 1.0, 0.5};
    numDrawCalls += drawPatches(patches, color);

    glEndQuery(GL_PRIMITIVES_GENERATED);
    glEndQuery(GL_TIME_ELAPSED);

    glBindVertexArray(0);

    glUseProgram(0);

    GLuint numPrimsGenerated = 0;
    GLuint timeElapsed = 0;
    glGetQueryObjectuiv(g_queries[0], GL_QUERY_RESULT, &numPrimsGenerated);
    glGetQueryObjectuiv(g_queries[1], GL_QUERY_RESULT, &timeElapsed);

    if (g_hud.IsVisible()) {
        g_hud.DrawString(10, -180, "Tess level  : %d", g_tessLevel);
        g_hud.DrawString(10, -160, "Primitives  : %d", numPrimsGenerated);
        g_hud.DrawString(10, -140, "Draw calls  : %d", numDrawCalls);

        g_hud.Flush();
    }

    glFinish();
}

//------------------------------------------------------------------------------
static void
motion(GLFWwindow *, double dx, double dy)
{
    int x=(int)dx, y=(int)dy;
    if (g_mbutton[0] && !g_mbutton[1] && !g_mbutton[2]) {
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
mouse(GLFWwindow *, int button, int state, int /* mods */)
{
    if (button == 0 && state == GLFW_PRESS && g_hud.MouseClick(g_prev_x, g_prev_y))
        return;

    if (button < 3) {
        g_mbutton[button] = (state == GLFW_PRESS);
    }
}

//------------------------------------------------------------------------------
static void
uninitGL()
{
    glDeleteQueries(2, g_queries);
    glDeleteVertexArrays(1, &g_vao);

    if (g_mesh)
        delete g_mesh;
}

//------------------------------------------------------------------------------
static void
reshape(GLFWwindow *, int width, int height)
{
    g_width = width;
    g_height = height;

    int windowWidth = g_width, windowHeight = g_height;
    // window size might not match framebuffer size on a high DPI display
    glfwGetWindowSize(g_window, &windowWidth, &windowHeight);
    g_hud.Rebuild(windowWidth, windowHeight);
}

//------------------------------------------------------------------------------
void windowClose(GLFWwindow*)
{
    g_running = false;
}

int windowClose()
{
    g_running = false;
    return GL_TRUE;
}

static void
rebuildOsdMesh()
{
    delete g_mesh;
    g_mesh = new Mesh(catmark_cube);
    g_mesh->SetRefineLevel(g_level);

    g_mesh->UpdateGeom();
}

//------------------------------------------------------------------------------
static void
keyboard(GLFWwindow *, int key, int /* scancode */, int event, int /* mods */)
{
    if (event == GLFW_RELEASE) return;
    if (g_hud.KeyDown(tolower(key))) return;

    if (key == 'G') {
        g_mesh->UpdateGeom();
    }

    switch (key) {
        case 'Q': g_running = 0; break;
        case 'F': fitFrame(); break;
        case '+':
        case '=': g_tessLevel++; break;
        case '-': g_tessLevel = std::max(g_tessLevelMin, g_tessLevel-1); break;
        case GLFW_KEY_ESCAPE: g_hud.SetVisible(!g_hud.IsVisible()); break;
    }
}

//------------------------------------------------------------------------------

static void
callbackLevel(int l)
{
    g_level = l;
    rebuildOsdMesh();
}

static void
callbackDisplayStyle(int b)
{
    g_displayStyle = b;
}

static void
callbackCheckBox(bool checked, int button)
{
    switch (button) {
    }
}

static void
initHUD()
{
    int windowWidth = g_width, windowHeight = g_height;
    // window size might not match framebuffer size on a high DPI display
    glfwGetWindowSize(g_window, &windowWidth, &windowHeight);
    g_hud.Init(windowWidth, windowHeight);

    int shading_pulldown = g_hud.AddPullDown("Shading (W)", 10, 10, 250, callbackDisplayStyle, 'w');
    g_hud.AddPullDownButton(shading_pulldown, "Wire", kWire, g_displayStyle==kWire);
    g_hud.AddPullDownButton(shading_pulldown, "Shaded", kShaded, g_displayStyle==kShaded);
    g_hud.AddPullDownButton(shading_pulldown, "Wire+Shaded", kWireShaded, g_displayStyle==kWireShaded);

    for (int i = 1; i < 11; ++i) {
        char level[16];
        sprintf(level, "Lv. %d", i);
        g_hud.AddRadioButton(3, level, i==2, 10, 210+i*20, callbackLevel, i, '0'+(i%10));
    }
}

//------------------------------------------------------------------------------
static void
initGL()
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glGenQueries(2, g_queries);

    glGenVertexArrays(1, &g_vao);
}

//------------------------------------------------------------------------------
static void
callbackError(OpenSubdiv::OsdErrorType err, const char *message)
{
    printf("OsdError: %d\n", err);
    printf("%s", message);
}

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

//------------------------------------------------------------------------------
int main(int argc, char ** argv)
{
    std::string str;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-d")) {
            g_level = atoi(argv[++i]);
        }
    }
    OsdSetErrorCallback(callbackError);

    if (not glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return 1;
    }

    static const char windowTitle[] = "OpenSubdiv face partitioning example";

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

    initGL();

    glfwSwapInterval(0);

    initHUD();
    rebuildOsdMesh();

    while (g_running) {
        display();

        glfwPollEvents();
        glfwSwapBuffers(g_window);

        glFinish();
    }

    uninitGL();
    glfwTerminate();
}

//------------------------------------------------------------------------------
