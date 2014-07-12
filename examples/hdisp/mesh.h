#ifndef MESH_H
#define MESH_H

#include <far/kernelBatch.h>
#include <osd/cpuComputeContext.h>
#include <osd/cpuGLVertexBuffer.h>
#include <osd/glDrawContext.h>

class HDisplacement;

class Mesh {
public:
    Mesh(const std::string &shape);

    ~Mesh();

    void SetRefineLevel(int level);

    OpenSubdiv::OsdGLDrawContext *GetDrawContext() const { return _drawContext; };

    GLuint BindVertexBuffer();

    void UpdateGeom();

    HDisplacement *GetHDisplacement() const {
        return _hdisp;
    }

private:
    OpenSubdiv::OsdCpuGLVertexBuffer *_vertexBuffer;
    OpenSubdiv::OsdCpuComputeContext *_computeContext;
    OpenSubdiv::OsdGLDrawContext *_drawContext;
    OpenSubdiv::FarKernelBatchVector _kernelBatches;
    std::string _shape;

    std::vector<float> _orgPositions;
    std::vector<float> _positions;

    HDisplacement *_hdisp;
};

#endif // MESH_H
