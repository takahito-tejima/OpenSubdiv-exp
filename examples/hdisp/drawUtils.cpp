#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>

#include "drawUtils.h"

struct Program
{
    GLuint program;
    GLuint uniformModelViewProjectionMatrix;
    GLuint attrPosition;
    GLuint attrColor;
} g_defaultProgram;

void
DrawUtils::SetModelViewProjection(const float *mvp)
{
    glProgramUniformMatrix4fv(g_defaultProgram.program,
                              g_defaultProgram.uniformModelViewProjectionMatrix,
                              1, GL_FALSE, mvp);
}

void
DrawUtils::Bind()
{
    glUseProgram(g_defaultProgram.program);
//    glEnableVertexAttribArray(1);
}

void
DrawUtils::Unbind()
{
    glUseProgram(0);
//    glDisableVertexAttribArray(0);
//    glDisableVertexAttribArray(1);
}

static GLuint
compileShader(GLenum shaderType, const char *source)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

bool
DrawUtils::Init()
{
#define GLSL_VERSION_DEFINE "#version 400\n"

    static const char *vsSrc =
        GLSL_VERSION_DEFINE
        "in vec3 position;\n"
        "out vec4 fragColor;\n"
        "uniform mat4 ModelViewProjectionMatrix;\n"
        "void main() {\n"
        "  fragColor = vec4(0, 1, 0, 1);\n"
        "  vec3 p = position;\n"
        "  gl_Position = ModelViewProjectionMatrix * "
        "                  vec4(p, 1);\n"
        "}\n";

    static const char *fsSrc =
        GLSL_VERSION_DEFINE
        "in vec4 fragColor;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  color = fragColor;\n"
        "}\n";

    GLuint program = glCreateProgram();
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSrc);

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint infoLogLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
        char *infoLog = new char[infoLogLength];
        glGetProgramInfoLog(program, infoLogLength, NULL, infoLog);
        printf("%s\n", infoLog);
        delete[] infoLog;
        exit(1);
    }

    g_defaultProgram.program = program;
    g_defaultProgram.uniformModelViewProjectionMatrix =
        glGetUniformLocation(program, "ModelViewProjectionMatrix");
    g_defaultProgram.attrPosition = glGetAttribLocation(program, "position");
    g_defaultProgram.attrColor = glGetAttribLocation(program, "color");

    return true;
}
