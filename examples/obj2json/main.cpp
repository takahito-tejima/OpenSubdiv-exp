#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include <far/topologyRefiner.h>
#include <far/patchTablesFactory.h>
#include <far/stencilTablesFactory.h>
#include <common/vtr_utils.h>
#include <common/shape_utils.h>

using namespace OpenSubdiv;

int main(int argc, char *argv[])
{
    int level = 1;
    bool yUp = true;
    const char *filename = NULL;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-l")) {
            level = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-z")) {
            yUp = false;
        } else {
            filename = argv[i];
        }
    }

    if (filename == NULL) {
        printf("Usage: %s -l <level> filename.obj\n", argv[0]);
        return 0;
    }

    std::ifstream ifs(filename);

    std::stringstream ss;
    ss << ifs.rdbuf();
    ifs.close();
    std::string str = ss.str();

    typedef OpenSubdiv::Far::ConstIndexArray IndexArray;

    Shape const * shape = Shape::parseObj(str.c_str(), kCatmark);

    // create Vtr mesh (topology)
    OpenSubdiv::Sdc::SchemeType sdctype = GetSdcType(*shape);
    OpenSubdiv::Sdc::Options sdcoptions = GetSdcOptions(*shape);

    OpenSubdiv::Far::TopologyRefiner * refiner =
        OpenSubdiv::Far::TopologyRefinerFactory<Shape>::Create(*shape,
            OpenSubdiv::Far::TopologyRefinerFactory<Shape>::Options(sdctype, sdcoptions));

    Far::TopologyRefiner::AdaptiveOptions options(level);
    options.fullTopologyInLastLevel = false;//fullTopologyInLastLevel;
    //    options.useSingleCreasePatch = singleCreasePatch;
    refiner->RefineAdaptive(options);

    Far::StencilTablesFactory::Options stOptions;
    stOptions.generateOffsets=true;
    stOptions.generateIntermediateLevels = refiner->IsUniform() ? false : true;
    const Far::StencilTables *vertexStencils =
        Far::StencilTablesFactory::Create(*refiner, stOptions);

    Far::KernelBatch kernelBatch =
        Far::StencilTablesFactory::Create(*vertexStencils);

    Far::PatchTablesFactory::Options ptOptions(level);
    //ptOptions.generateFVarTables = bits.test(MeshFVarData);
    //ptOptions.useSingleCreasePatch = bits.test(MeshUseSingleCreasePatch);

    Far::PatchTables *patchTables = Far::PatchTablesFactory::Create(*refiner);

    Far::PatchTables::PatchVertsTable vertsTable = patchTables->GetPatchControlVerticesTable();

    int nTotalVerts = shape->verts.size() + vertexStencils->GetNumStencils();

    const char *VTX = nTotalVerts > 32767 ? "32" : "16";

    printf("{\n");
    printf("  \"model\": {\n");

    // bsplines
    printf("    \"patches\": new Int%sArray([\n", VTX);
    int narrays = patchTables->GetNumPatchArrays();
    for (int array=0; array<narrays; ++array) {
        Far::PatchDescriptor srcDesc = patchTables->GetPatchArrayDescriptor(array);
        if (srcDesc.GetType() == Far::PatchDescriptor::GREGORY ||
            srcDesc.GetType() == Far::PatchDescriptor::GREGORY_BOUNDARY) continue;

        int npatches = patchTables->GetNumPatches(array);
        for (int j = 0; j < npatches; ++j) {
            Far::ConstIndexArray indices =
                patchTables->GetPatchVertices(array, j);
            for (int i = 0; i < 16; ++i) {
                printf("%d,", i < indices.size() ? indices[i] : -1);
            }
            printf("\n");
        }
    }
    printf("    ]),\n");

    // gregory
    printf("    \"gregoryPatches\": new Int%sArray([\n", VTX);
    for (int array=0; array<narrays; ++array) {
        Far::PatchDescriptor srcDesc = patchTables->GetPatchArrayDescriptor(array);
        if (srcDesc.GetType() != Far::PatchDescriptor::GREGORY &&
            srcDesc.GetType() != Far::PatchDescriptor::GREGORY_BOUNDARY) continue;

        int npatches = patchTables->GetNumPatches(array);
        for (int j = 0; j < npatches; ++j) {
            Far::ConstIndexArray indices =
                patchTables->GetPatchVertices(array, j);
            for (int i = 0; i < indices.size(); ++i) {
                printf("%d,", indices[i]);
            }
            printf("\n");
        }
    }
    printf("    ]),\n");

    printf("    \"patchParams\": new Uint8Array([\n");
    for (int array=0; array<narrays; ++array) {
        Far::PatchDescriptor srcDesc = patchTables->GetPatchArrayDescriptor(array);
        if (srcDesc.GetType() == Far::PatchDescriptor::GREGORY ||
            srcDesc.GetType() == Far::PatchDescriptor::GREGORY_BOUNDARY) continue;

        int npatches = patchTables->GetNumPatches(array);
        for (int j = 0; j < npatches; ++j) {
            Far::PatchParam param = patchTables->GetPatchParam(array, j);
            printf("%d, %d, %d, %d, %d,",
                   (int)param.bitField.GetDepth(),
                   param.bitField.GetRotation(),
                   srcDesc.GetType(),
                   srcDesc.GetPattern(),
                   srcDesc.GetRotation());
            printf("\n");
        }
    }
    for (int array=0; array<narrays; ++array) {
        Far::PatchDescriptor srcDesc = patchTables->GetPatchArrayDescriptor(array);
        if (srcDesc.GetType() != Far::PatchDescriptor::GREGORY &&
            srcDesc.GetType() != Far::PatchDescriptor::GREGORY_BOUNDARY) continue;

        int npatches = patchTables->GetNumPatches(array);
        for (int j = 0; j < npatches; ++j) {
            Far::PatchParam param = patchTables->GetPatchParam(array, j);
            printf("%d, %d, %d, %d, %d,",
                   (int)param.bitField.GetDepth(),
                   param.bitField.GetRotation(),
                   srcDesc.GetType(),
                   srcDesc.GetPattern(),
                   srcDesc.GetRotation());
            printf("\n");
        }
    }
    printf("    ]),\n");


    printf("    \"vertexValences\": new Int%sArray([\n", VTX);
    {
        Far::PatchTables::VertexValenceTable valenceTable = patchTables->GetVertexValenceTable();
        for(int i = 0; i < valenceTable.size(); ++i) {
            printf("%d,", valenceTable[i]);
        }
    }
    printf("\n    ]),\n");
    printf("    \"maxValence\": %d,\n", patchTables->GetMaxValence());
    printf("    \"quadOffsets\": new Int16Array([\n");
    {
        Far::PatchTables::QuadOffsetsTable quadOffsets = patchTables->GetQuadOffsetsTable();
        for(int i = 0; i < quadOffsets.size(); ++i) {
            printf("%d,", quadOffsets[i]);
        }
    }
    printf("\n    ]),\n");


    int nStencils = vertexStencils->GetNumStencils();
    int stencilIndex = 0;
    printf("    \"stencils\": new Uint32Array([\n");
    for (int i = 0; i < nStencils; ++i) {
        Far::Stencil stencil = vertexStencils->GetStencil(i);
        int size = stencil.GetSize();
        printf("%d, %d,", stencilIndex, size);
        stencilIndex += size;
    }
    printf("\n    ]),\n");

    printf("    \"stencilIndices\": new Uint%sArray([\n", VTX);
    printf("      ");
    for (int i = 0; i < nStencils; ++i) {
        Far::Stencil stencil = vertexStencils->GetStencil(i);
        int size = stencil.GetSize();
        for(int j = 0; j < size; ++j) {
            printf("%d,", stencil.GetVertexIndices()[j]);
        }
    }
    printf("\n    ]),\n");
    printf("    \"stencilWeights\": new Float32Array([\n");
    printf("      ");
    for (int i = 0; i < nStencils; ++i) {
        Far::Stencil stencil = vertexStencils->GetStencil(i);
        int size = stencil.GetSize();
        for(int j = 0; j < size; ++j) {
            printf("%f,", stencil.GetWeights()[j]);
        }
    }
    printf("\n    ]),\n");

    printf("    \"points\": new Float32Array([\n");
    for (int i = 0; i < shape->verts.size(); i += 3) {
        if (yUp) {
            printf("%f, %f, %f, ", shape->verts[i+0], shape->verts[i+1], shape->verts[i+2]);
        } else {
            printf("%f, %f, %f, ", shape->verts[i+0], shape->verts[i+2], -shape->verts[i+1]);
        }
    }
    printf("\n    ]),\n");

    printf("    \"hull\": new Int32Array([\n");
    int vid = 0;
    for (int i = 0; i < shape->nvertsPerFace.size(); ++i) {
        int nverts = shape->nvertsPerFace[i];
        for (int j = 0; j < nverts; ++j) {
            printf("%d,%d,", shape->faceverts[vid+j],
                   shape->faceverts[vid+(j+1)%nverts]);
        }
        vid += nverts;
    }
    printf("])\n");

    printf("  }\n");
    printf("}\n");
}
