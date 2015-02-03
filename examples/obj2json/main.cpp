#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <float.h>

#include <far/topologyRefiner.h>
#include <far/patchTablesFactory.h>
#include <far/stencilTablesFactory.h>
#include <common/vtr_utils.h>
#include <common/shape_utils.h>

#include <osd/ptexMipmapTextureLoader.h>

#include "Ptexture.h"
#include "PtexUtils.h"

#include <zlib.h>
#include <png.h>

using namespace OpenSubdiv;

void writePNG(const char *filename, int width, int height, int nChannel, const unsigned char *buf)
{
    if (FILE * f = fopen(filename, "w" )) {

        png_structp png_ptr =
            png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        assert(png_ptr);

        png_infop info_ptr =
            png_create_info_struct(png_ptr);
        assert(info_ptr);

        int type = PNG_COLOR_TYPE_RGB_ALPHA;
        int depth = 8;
        if (nChannel == 3) type = PNG_COLOR_TYPE_RGB;
        else if (nChannel == 1) {
            type = PNG_COLOR_TYPE_GRAY;
        }

        png_set_IHDR(png_ptr, info_ptr, width, height, depth,
                     type,
                         PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_DEFAULT,
                         PNG_FILTER_TYPE_DEFAULT );

        png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

        png_bytep rows_ptr[ height ];
        for(int i = 0; i<height; ++i ) {
            //rows_ptr[height-i-1] = ((png_byte *)buf) + i*width*4;
            rows_ptr[i] = ((png_byte *)buf) + i*width*nChannel;
        }

        png_set_rows(png_ptr, info_ptr, rows_ptr);

        png_init_io(png_ptr, f);

        png_write_png( png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0 );

        png_destroy_write_struct(&png_ptr, &info_ptr);

        fclose(f);
    } else {
        fprintf(stderr, "Error creating: %s\n", filename);
    }
}

void createPtex(PtexTexture *reader, const char *texname, bool png)
{
    int maxNumPages = 1;
    int maxLevels = 6;
    int targetMemory = 4096*4096;

    // Read the ptexture data and pack the texels
    Osd::PtexMipmapTextureLoader loader(reader,
                                   maxNumPages,
                                   maxLevels,
                                   targetMemory);

    // Setup GPU memory
    int numFaces = loader.GetNumFaces();
    int width = loader.GetPageWidth();
    int height = loader.GetPageHeight();
    int depth = loader.GetNumPages();
    int channel = reader->numChannels();
    int type = reader->dataType();;

    printf("    \"ptexDim_%s\": [%d, %d, %d, %d, %d],\n", texname, width, height, depth, channel, type);
    fprintf(stderr, "%d * %d * %d, %d, %d\n", width, height, depth, channel, type);

    printf("    \"ptexLayout_%s\": new Uint16Array([\n", texname);
    int layoutSize = numFaces * 6;
    const unsigned short *layout = (const unsigned short*)loader.GetLayoutBuffer();
    for (int i = 0; i < layoutSize; ++i) {
        printf("%d,", layout[i]);
    }
    printf("    ]),\n");


    const unsigned char *texel = loader.GetTexelBuffer();
    if (png) {
        char filename[256];
        sprintf(filename, "%s.png", texname);
        fprintf(stderr, "writing %s\n", filename);
        writePNG(filename, width, height, channel, texel);
    } else {
        char filename[256];
        sprintf(filename, "%s.raw", texname);
        fprintf(stderr, "writing %s\n", filename);

        FILE *fp = fopen(filename, "wb");

        int bpp[] = {1, 2, 2, 4}; // uin8, uin16, half, float

        int size = width*height*channel*bpp[type];
        fwrite(texel, size, 1, fp);
        fclose(fp);
    }

    /*
    unsigned int layout = genTextureBuffer(GL_R16I,
                                           numFaces * 6 * sizeof(GLshort),
                                           loader.GetLayoutBuffer());

    GLenum format, type;
    switch (reader->dataType()) {
        case Ptex::dt_uint16 : type = GL_UNSIGNED_SHORT; break;
        case Ptex::dt_float  : type = GL_FLOAT; break;
        case Ptex::dt_half   : type = GL_HALF_FLOAT; break;
        default              : type = GL_UNSIGNED_BYTE; break;
    }

    switch (reader->numChannels()) {
        case 1 : format = GL_RED; break;
        case 2 : format = GL_RG; break;
        case 3 : format = GL_RGB; break;
        case 4 : format = GL_RGBA; break;
        default: format = GL_RED; break;
    }

    // actual texels texture array
    GLuint texels;
    glGenTextures(1, &texels);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texels);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0,
                 (type == GL_FLOAT) ? GL_RGBA32F : GL_RGBA,
                 loader.GetPageWidth(),
                 loader.GetPageHeight(),
                 loader.GetNumPages(),
                 0, format, type,
                 loader.GetTexelBuffer());

//    loader.ClearBuffers();

    // Return the Osd Ptexture object
    result = new GLPtexMipmapTexture;

    result->_width = loader.GetPageWidth();
    result->_height = loader.GetPageHeight();
    result->_depth = loader.GetNumPages();

    result->_format = format;

    result->_layout = layout;
    result->_texels = texels;
    result->_memoryUsage = loader.GetMemoryUsage();

    return result;
    */
}

Shape *
createPTexGeo(PtexTexture * r) {

    PtexMetaData* meta = r->getMetaData();

    if (meta->numKeys() < 3) {
        return NULL;
    }

    float const * vp;
    int const *vi, *vc;
    int nvp, nvi, nvc;

    meta->getValue("PtexFaceVertCounts", vc, nvc);
    if (nvc == 0) {
        return NULL;
    }
    meta->getValue("PtexVertPositions", vp, nvp);
    if (nvp == 0) {
        return NULL;
    }
    meta->getValue("PtexFaceVertIndices", vi, nvi);
    if (nvi == 0) {
        return NULL;
    }

    Shape * shape = new Shape;

    shape->scheme = kCatmark;

    shape->verts.resize(nvp);
    for (int i=0; i<nvp; ++i) {
        shape->verts[i] = vp[i];
    }

    shape->nvertsPerFace.resize(nvc);
    for (int i=0; i<nvc; ++i) {
        shape->nvertsPerFace[i] = vc[i];
    }

    shape->faceverts.resize(nvi);
    for (int i=0; i<nvi; ++i) {
        shape->faceverts[i] = vi[i];
    }

    return shape;
}
void centering(Shape *shape)
{
    // compute model bounding
    float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (size_t i = 0; i < shape->verts.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            min[j] = std::min(min[j], shape->verts[i+j]);
            max[j] = std::max(max[j], shape->verts[i+j]);
        }
    }
    float diag[3], center[3];
    for (int j = 0; j < 3; ++j) {
        diag[j] = max[j]-min[j];
        center[j] = (max[j]+min[j])*0.5;
    }
    float rad = sqrt(diag[0]*diag[0] + diag[1]*diag[1] + diag[2]*diag[2]);

    for (size_t i = 0; i < shape->verts.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            float v = (shape->verts[i+j] - center[j])/rad;
            shape->verts[i+j] = v;
        }
    }
}

int main(int argc, char *argv[])
{
    int level = 1;
    bool yUp = true;
    const char *filename = NULL;
    const char *displaceFileName = NULL;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-l")) {
            level = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-z")) {
            yUp = false;
        } else if (filename == NULL) {
            filename = argv[i];
        } else if (displaceFileName == NULL) {
            displaceFileName = argv[i];
        }
    }

    if (filename == NULL) {
        printf("Usage: %s -l <level> filename.{obj|ptx} [displace.ptx]\n", argv[0]);
        return 0;
    }

    PtexTexture *ptexColor = NULL;
    Ptex::String ptexError;
    Shape *shape = NULL;

#define USE_PTEX_CACHE
#define PTEX_CACHE_SIZE (512*1024*1024)
    PtexCache *cache = PtexCache::create(1, PTEX_CACHE_SIZE);

    if (strstr(filename, ".ptx") != NULL) {
        PtexTexture *ptex = cache->get(filename, ptexError);
        shape = createPTexGeo(ptex);
        ptexColor = ptex;
    } else {
        std::ifstream ifs(filename);

        std::stringstream ss;
        ss << ifs.rdbuf();
        ifs.close();
        std::string str = ss.str();

        shape = Shape::parseObj(str.c_str(), kCatmark);
    }

    // centering & normalize
    centering(shape);

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

    printf("    \"patchParams\": new Uint16Array([\n");
    for (int array=0; array<narrays; ++array) {
        Far::PatchDescriptor srcDesc = patchTables->GetPatchArrayDescriptor(array);
        if (srcDesc.GetType() == Far::PatchDescriptor::GREGORY ||
            srcDesc.GetType() == Far::PatchDescriptor::GREGORY_BOUNDARY) continue;

        int npatches = patchTables->GetNumPatches(array);
        for (int j = 0; j < npatches; ++j) {
            Far::PatchParam param = patchTables->GetPatchParam(array, j);
            printf("%d, %d, %d, %d, %d, %d, %d, %d,",
                   (int)param.bitField.GetDepth(),
                   param.bitField.GetRotation(),
                   srcDesc.GetType(),
                   srcDesc.GetPattern(),
                   srcDesc.GetRotation(),
                   param.bitField.GetU(),
                   param.bitField.GetV(),
                   param.faceIndex);
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
            printf("%d, %d, %d, %d, %d, %d, %d, %d,",
                   (int)param.bitField.GetDepth(),
                   param.bitField.GetRotation(),
                   srcDesc.GetType(),
                   srcDesc.GetPattern(),
                   srcDesc.GetRotation(),
                   param.bitField.GetU(),
                   param.bitField.GetV(),
                   param.faceIndex);
            printf("\n");
        }
    }
    printf("    ]),\n");


    printf("    \"vertexValences\": new Int%sArray([\n", VTX);
    {
        Far::PatchTables::VertexValenceTable valenceTable = patchTables->GetVertexValenceTable();
        for(size_t i = 0; i < valenceTable.size(); ++i) {
            printf("%d,", valenceTable[i]);
        }
    }
    printf("\n    ]),\n");
    printf("    \"maxValence\": %d,\n", patchTables->GetMaxValence());
    printf("    \"quadOffsets\": new Int16Array([\n");
    {
        Far::PatchTables::QuadOffsetsTable quadOffsets = patchTables->GetQuadOffsetsTable();
        for(size_t i = 0; i < quadOffsets.size(); ++i) {
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
    for (size_t i = 0; i < shape->verts.size(); i += 3) {
        if (yUp) {
            printf("%f, %f, %f, ", shape->verts[i+0], shape->verts[i+1], shape->verts[i+2]);
        } else {
            printf("%f, %f, %f, ", shape->verts[i+0], shape->verts[i+2], -shape->verts[i+1]);
        }
    }
    printf("\n    ]),\n");

    printf("    \"hull\": new Int32Array([\n");
    int vid = 0;
    for(size_t i = 0; i < shape->nvertsPerFace.size(); ++i) {
        int nverts = shape->nvertsPerFace[i];
        for (int j = 0; j < nverts; ++j) {
            printf("%d,%d,", shape->faceverts[vid+j],
                   shape->faceverts[vid+(j+1)%nverts]);
        }
        vid += nverts;
    }
    printf("]),\n");

    if (ptexColor) {
        createPtex(ptexColor, "color", true);
    }
    if (displaceFileName) {
        //PtexTexture *ptex = PtexTexture::open(displaceFileName, ptexError, true);
        PtexTexture *ptex = cache->get(displaceFileName, ptexError);
        if (ptex == NULL) {
            fprintf(stderr, "Error in open %s\n", displaceFileName);
        }
        createPtex(ptex, "displace", false);
    }

    printf("  }\n");
    printf("}\n");
}
