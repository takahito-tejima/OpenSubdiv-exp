#include "hdisp.h"

HDisplacement::HDisplacement(OsdHbrMesh *hbrMesh)
    : _layout(0), _texels(0)
{
    // count num ptex faces;
    int numFaces = hbrMesh->GetNumFaces();
    _numPtexFaces = 0;
    for (int i = 0; i < numFaces; ++i) {
        OsdHbrFace *face = hbrMesh->GetFace(i);
        int nv = face->GetNumVertices();
        if (nv == 4) {
            ++_numPtexFaces;
        } else {
            _numPtexFaces += nv;
        }
    }

    _faceRes = 4; // 1<<4 = 16
    int res = 1 << _faceRes;
    int width = res + 2 + res/2 + 2;
    int height = res + 2;
    _width = width;
    _height = height * _numPtexFaces;

    _texels.resize(_width*_height);

    // init pattern
    for (int i = 0; i < _numPtexFaces; ++i) {
        Layout layout;
        layout.page = 0; // page
        layout.nMipmap = _faceRes-1; // nMipmap
        layout.u = 0; // u
        layout.v = height*i; // v
        layout.adjSizeDiffs = 0; // adjSizeDiffs
        layout.width = _faceRes;
        layout.height = _faceRes;
        _layout.push_back(layout);

        // for (int u = 0; u < width; ++u) {
        //     for (int v = 0; v < height; ++v) {
        //         _texels[(i*height+v)*_width+u] = (u&1)^(v&1);
        //     }
        // }
    }

    glGenTextures(1, &_layoutTexture);
    glGenTextures(1, &_texelsTexture);

    // layout buffer
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);

    glBindBuffer(GL_TEXTURE_BUFFER, buffer);
    glBufferData(GL_TEXTURE_BUFFER, _numPtexFaces*6*2, &_layout[0], GL_STATIC_DRAW);
    glBindTexture(GL_TEXTURE_BUFFER, _layoutTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R16I, buffer);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    glDeleteBuffers(1, &buffer);

    // texel buffer
    glBindTexture(GL_TEXTURE_2D_ARRAY, _texelsTexture);
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
                 &_texels[0]);
}

HDisplacement::~HDisplacement()
{
    if (_layoutTexture) glDeleteTextures(1, &_layoutTexture);
    if (_texelsTexture) glDeleteTextures(1, &_texelsTexture);
}

int
HDisplacement::BindTexture(GLint program, GLint data, GLint packing, int samplerUnit)
{
    glProgramUniform1i(program, data, samplerUnit + 0);
    glProgramUniform1i(program, packing, samplerUnit + 1);

    glActiveTexture(GL_TEXTURE0 + samplerUnit + 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _texelsTexture);

    glActiveTexture(GL_TEXTURE0 + samplerUnit + 1);
    glBindTexture(GL_TEXTURE_BUFFER, _layoutTexture);

    glActiveTexture(GL_TEXTURE0);

    return samplerUnit + 2;
}

static int computeMipmapOffsetU(int w, int level)
{
    int width = 1 << w;
    int m = (0x55555555 & (width | (width-1))) << (w&1);
    int x = ~((1 << (w -((level-1)&~1))) - 1);
    return (m & x) + ((level+1)&~1);
}
static int computeMipmapOffsetV(int h, int level)
{
    int height = 1 << h;
    int m = (0x55555555 & (height-1)) << ((h+1)&1);;
    int x = ~((1 << (h - (level&~1))) - 1 );
    return (m & x) + (level&~1);
}

void
HDisplacement::ApplyEdit(int face, int level, int index, float value)
{
    // (1<<level)*(1<<level) indices.
    if (face < 0 || face >= _numPtexFaces) return;
    if (index < 0 || index >= (1<<level)*(1<<level)) return;

    int res = 1 << _faceRes;
    int width = res + 2 + res/2 + 2;
    int height = res + 2;

    int u = computeMipmapOffsetU(_faceRes, level);
    int v = computeMipmapOffsetV(_faceRes, level) + face*height;
    u++;
    v++;
    // add index;

    _texels[v*_width+u] += value;

    glBindTexture(GL_TEXTURE_2D_ARRAY, _texelsTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0,
                 GL_RGBA32F,
                 _width,
                 _height,
                 1,
                 0, GL_RED, GL_FLOAT,
                 &_texels[0]);
}
