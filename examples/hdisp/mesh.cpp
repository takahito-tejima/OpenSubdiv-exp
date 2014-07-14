#include <climits>
#include <cfloat>
#include <far/mesh.h>
#include <far/meshFactory.h>
#include <osd/vertex.h>
#include <osd/cpuComputeController.h>
#include <common/shape_utils.h>
#include "mesh.h"
#include "hdisp.h"
#include "drawUtils.h"

typedef OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex>     OsdHbrMesh;
typedef OpenSubdiv::HbrVertex<OpenSubdiv::OsdVertex>   OsdHbrVertex;
typedef OpenSubdiv::HbrFace<OpenSubdiv::OsdVertex>     OsdHbrFace;
typedef OpenSubdiv::HbrHalfedge<OpenSubdiv::OsdVertex> OsdHbrHalfedge;

Mesh::Mesh(const std::string &shape) :
    _vertexBuffer(NULL), _computeContext(NULL), _drawContext(NULL),
    _shape(shape), _hdisp(NULL)
{
    OsdHbrMesh * hmesh = simpleHbr<OpenSubdiv::OsdVertex>(
        _shape.c_str(), kCatmark, _orgPositions);

    // allocate ptex
    _hdisp = new HDisplacement(hmesh);

    glGenVertexArrays(1, &_cageEdgeVAO);
    glGenBuffers(1, &_cageEdgeVBO);
}

Mesh::~Mesh()
{
    delete _vertexBuffer;
    delete _computeContext;
    delete _drawContext;
    delete _hdisp;
}

void
Mesh::SetRefineLevel(int level)
{
    using namespace OpenSubdiv;

    delete _vertexBuffer;
    delete _computeContext;
    delete _drawContext;

    // create again for adaptive refinement.
    std::vector<float> tmp;
    OsdHbrMesh * hmesh = simpleHbr<OsdVertex>(_shape.c_str(), kCatmark, tmp);

    // centering rest position
    float min[3] = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    float max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    float center[3];
    for (size_t i=0; i < _orgPositions.size()/3; ++i) {
        for (int j=0; j<3; ++j) {
            float v = _orgPositions[i*3+j];
            min[j] = std::min(min[j], v);
            max[j] = std::max(max[j], v);
        }
    }
    for (int j=0; j<3; ++j) center[j] = (min[j] + max[j]) * 0.5f;
    for (size_t i=0; i < _orgPositions.size()/3; ++i) {
        _orgPositions[i*3+0] -= center[0];
        _orgPositions[i*3+1] -= center[1];
        _orgPositions[i*3+2] -= center[2];
    }
    _positions.resize(_orgPositions.size());

    // save coarse topology (used for coarse mesh drawing)
    _coarseEdges.clear();
    int nf = hmesh->GetNumFaces();
    for(int i=0; i<nf; ++i) {
        OsdHbrFace *face = hmesh->GetFace(i);
        int nv = face->GetNumVertices();
        for(int j=0; j<nv; ++j) {
            _coarseEdges.push_back(face->GetVertex(j)->GetID());
            _coarseEdges.push_back(face->GetVertex((j+1)%nv)->GetID());
        }
    }

    // FAR
    FarMeshFactory<OsdVertex> meshFactory(hmesh, level, true);
    FarMesh<OsdVertex> *farMesh = meshFactory.Create();

    int numVertices = farMesh->GetNumVertices();

    _vertexBuffer = OsdCpuGLVertexBuffer::Create(3, numVertices);
    _computeContext = OsdCpuComputeContext::Create(
        farMesh->GetSubdivisionTables(), farMesh->GetVertexEditTables());
    _drawContext = OsdGLDrawContext::Create(farMesh->GetPatchTables(), 3, false);

    _kernelBatches = farMesh->GetKernelBatches();

    _drawContext->UpdateVertexTexture(_vertexBuffer);

    delete hmesh;
    delete farMesh;
}

void
Mesh::UpdateGeom(float deform)
{
    int nverts = (int)_orgPositions.size() / 3;

    float *d = &_positions[0];
    const float *p = &_orgPositions[0];

    float r = sin(deform*6.28);
    for (int j = 0; j < nverts; ++j) {
        float x = p[2]+0.5;
        float ct = sin(x * r);
        float st = cos(x * r);
        *d++ = p[0]*ct + p[1]*st;
        *d++ = -p[0]*st + p[1]*ct;
        *d++ = p[2];
        p += 3;
    }
    _vertexBuffer->UpdateData(&_positions[0], 0, nverts);

    static OpenSubdiv::OsdCpuComputeController controller;
    controller.Refine(_computeContext,
                      _kernelBatches,
                      _vertexBuffer);
}

GLuint
Mesh::BindVertexBuffer()
{
    return _vertexBuffer->BindVBO();
}

void
Mesh::DrawCage()
{
    std::vector<float> vbo;
    vbo.reserve(_coarseEdges.size() * 3);
    for (int i = 0; i < (int)_coarseEdges.size(); i+=2) {
        for (int j = 0; j < 2; ++j) {
            vbo.push_back(_positions[_coarseEdges[i+j]*3]);
            vbo.push_back(_positions[_coarseEdges[i+j]*3+1]);
            vbo.push_back(_positions[_coarseEdges[i+j]*3+2]);
        }
    }

    glBindVertexArray(_cageEdgeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, _cageEdgeVBO);
    glBufferData(GL_ARRAY_BUFFER, (int)vbo.size() * sizeof(float), &vbo[0],
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof (GLfloat) * 3, 0);

    glDrawArrays(GL_LINES, 0, (int)_coarseEdges.size());

    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
}
