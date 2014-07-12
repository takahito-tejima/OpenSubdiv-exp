#include "hdisp.h"

HDisplacement::HDisplacement(OsdHbrMesh *hbrMesh)
    : _layout(0), _texels(0)
{
    // count num ptex faces;
    int numFaces = hbrMesh->GetNumFaces();
    int numPtexFaces = 0;
    for (int i = 0; i < numFaces; ++i) {
        OsdHbrFace *face = hbrMesh->GetFace(i);
        int nv = face->GetNumVertices();
        if (nv == 4) {
            ++numPtexFaces;
        } else {
            numPtexFaces += nv;
        }
    }

    _faceRes = 4; // 1<<4 = 16
    int res = 1 << _faceRes;
    int width = res + 2 + res/2 + 2;
    int height = res + 2;
    _width = width;
    _height = height * numPtexFaces;

    // ptex layout struct
    // struct Layout {
    //     uint16_t page;
    //     uint16_t nMipmap;
    //     uint16_t u;
    //     uint16_t v;
    //     uint16_t adjSizeDiffs; //(4:4:4:4)
    //     uint8_t  width log2;
    //     uint8_t  height log2;
    // };

    std::vector<uint16_t> layout(numPtexFaces * 6);
    std::vector<float> texels(_width*_height);

    // init pattern
    for (int i = 0; i < numPtexFaces; ++i) {
        layout[i*6+0] = 0; // page
        layout[i*6+1] = _faceRes-2; // nMipmap
        layout[i*6+2] = 0; // u
        layout[i*6+3] = height*i; // v
        layout[i*6+4] = 0; // adjSizeDiffs
        layout[i*6+5] = (_faceRes<<8) | _faceRes; // width,height

        for (int u = 0; u < width; ++u) {
            for (int v = 0; v < height; ++v) {
                texels[(i*height+v)*_width+u] = u/(float)width;
            }
        }
    }

    glGenTextures(1, &_layout);
    glGenTextures(1, &_texels);

    // layout buffer
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);

    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, numPtexFaces*6*2, &layout[0], GL_STATIC_DRAW);
    glBindTexture(GL_TEXTURE_BUFFER, _layout);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R16I, buffer);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    glDeleteBuffers(1, &buffer);

    // texel buffer
    glBindTexture(GL_TEXTURE_2D_ARRAY, _texels);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0,
                 GL_RGBA32F,
                 _width,
                 _height,
                 1,
                 0, GL_RED, GL_FLOAT,
                 &texels[0]);
}

HDisplacement::~HDisplacement()
{
    if (_layout) glDeleteTextures(1, &_layout);
    if (_texels) glDeleteTextures(1, &_texels);
}

int
HDisplacement::BindTexture(GLint program, GLint data, GLint packing, int samplerUnit)
{
    glProgramUniform1i(program, data, samplerUnit + 0);
    glProgramUniform1i(program, packing, samplerUnit + 1);

    glActiveTexture(GL_TEXTURE0 + samplerUnit + 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _texels);

    glActiveTexture(GL_TEXTURE0 + samplerUnit + 1);
    glBindTexture(GL_TEXTURE_BUFFER, _layout);

    glActiveTexture(GL_TEXTURE0);

    return samplerUnit + 2;
}

