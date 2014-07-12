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

class HDisplacement {
public:
    HDisplacement(OsdHbrMesh *mesh);
    ~HDisplacement();

    /// Returns the texture buffer containing the layout of the ptex faces
    /// in the texels texture array.
    GLuint GetLayoutTextureBuffer() const { return _layout; }

    /// Returns the texels texture array.
    GLuint GetTexelsTexture() const { return _texels; }

    int BindTexture(GLint program, GLint texData, GLint texPacking, int sampler);

private:
    GLuint _layout;
    GLuint _texels;

    int _faceRes;
    int _width;
    int _height;

};

#endif  // HDISP_H
