#ifndef HDISP_H
#define HDISP_H

#include <GL/glew.h>
#include <far/mesh.h>
#include <far/meshFactory.h>
#include <osd/vertex.h>

typedef OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex>     OsdHbrMesh;
typedef OpenSubdiv::HbrVertex<OpenSubdiv::OsdVertex>   OsdHbrVertex;
typedef OpenSubdiv::HbrFace<OpenSubdiv::OsdVertex>     OsdHbrFace;
typedef OpenSubdiv::HbrHalfedge<OpenSubdiv::OsdVertex> OsdHbrHalfedge;

struct Edit {
    int face;
    int level;
    int index;
};

class HDisplacement {
public:
    HDisplacement(OsdHbrMesh *mesh);
    ~HDisplacement();

    /// Returns the texture buffer containing the layout of the ptex faces
    /// in the texels texture array.
    GLuint GetLayoutTextureBuffer() const { return _layoutTexture; }

    /// Returns the texels texture array.
    GLuint GetTexelsTexture() const { return _texelsTexture; }

    int BindTexture(GLint program, GLint texData, GLint texPacking, int sampler);

    int GetNumPtexFaces() const {
        return _numPtexFaces;
    }

    void ApplyEdit(int face, int level, int index, float value);

private:
    std::vector<Edit> _edits;
    GLuint _layoutTexture;
    GLuint _texelsTexture;

    struct Layout {
        uint16_t page;
        uint16_t nMipmap;
        uint16_t u;
        uint16_t v;
        uint16_t adjSizeDiffs; //(4:4:4:4)
        uint8_t  width;
        uint8_t  height;
    };
    std::vector<Layout> _layout;
    std::vector<float> _texels;

    int _faceRes;
    int _width;
    int _height;
    int _numPtexFaces;
};

#endif  // HDISP_H
