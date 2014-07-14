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
#line 24

//--------------------------------------------------------------
// Uniforms / Uniform Blocks
//--------------------------------------------------------------

layout(std140) uniform Transform {
    mat4 ModelViewMatrix;
    mat4 ProjectionMatrix;
    mat4 ModelViewProjectionMatrix;
};

layout(std140) uniform Tessellation {
    float TessLevel;
};

uniform int GregoryQuadOffsetBase;
uniform int PrimitiveIdBase;
uniform int BaseVertex;

//--------------------------------------------------------------
// Osd external functions
//--------------------------------------------------------------

#ifdef GL_ARB_shader_draw_parameters
#extension GL_ARB_shader_draw_parameters : enable
#endif

mat4 OsdModelViewMatrix()
{
    return ModelViewMatrix;
}
mat4 OsdProjectionMatrix()
{
    return ProjectionMatrix;
}
mat4 OsdModelViewProjectionMatrix()
{
    return ModelViewProjectionMatrix;
}
float OsdTessLevel()
{
    return TessLevel;
}
int OsdGregoryQuadOffsetBase()
{
    return GregoryQuadOffsetBase;
}
int OsdPrimitiveIdBase()
{
    return PrimitiveIdBase;
}
int OsdBaseVertex()
{

#ifdef GL_ARB_shader_draw_parameters
    // return gl_BaseVertexARB;
    return BaseVertex;
#else
    return BaseVertex;
#endif
}

uniform int hdispLevel;
uniform int superpose;
uniform sampler2DArray textureImage_Data;
uniform isamplerBuffer textureImage_Packing;

float
GetDisplacement(vec4 patchCoord)
{
    int maxMipmap = 5;
    float disp = 0;
    if (superpose == 1) {
        for (int i = 0; i < maxMipmap; ++i) {
            disp += PtexMipmapLookupQuadratic(patchCoord,
                                             i,
                                             textureImage_Data,
                                             textureImage_Packing).x;
        }
    } else {
        disp = PtexMipmapLookupQuadratic(patchCoord,
                                         hdispLevel,
                                         textureImage_Data,
                                         textureImage_Packing).x;
    }
    return disp;
}

//--------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------
#ifdef VERTEX_SHADER

layout (location=0) in vec4 position;

out block {
    OutputVertex v;
} outpt;

void main()
{
    outpt.v.position = ModelViewMatrix * position;
}

#endif

//--------------------------------------------------------------
// Geometry Shader
//--------------------------------------------------------------
#ifdef GEOMETRY_SHADER

#ifdef PRIM_QUAD

    layout(lines_adjacency) in;

    #define EDGE_VERTS 4

#endif // PRIM_QUAD

#ifdef  PRIM_TRI

    layout(triangles) in;

    #define EDGE_VERTS 3

#endif // PRIM_TRI


layout(triangle_strip, max_vertices = EDGE_VERTS) out;
in block {
    OutputVertex v;
} inpt[EDGE_VERTS];

out block {
    OutputVertex v;
    noperspective out vec4 edgeDistance;
} outpt;

uniform float displaceScale;

void emit(int index, vec3 point, vec3 normal)
{
// #ifdef SMOOTH_NORMALS
//     outpt.v.normal = inpt[index].v.normal;
// #else
//     outpt.v.normal = normal;
// #endif

    outpt.v.normal = normal;
    outpt.v.position = vec4(point,1);
    outpt.v.patchCoord = inpt[index].v.patchCoord;
    outpt.v.tangent = inpt[index].v.tangent;
    outpt.v.bitangent = inpt[index].v.bitangent;

    gl_Position = ProjectionMatrix * vec4(point, 1);
    EmitVertex();
}

#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
const float VIEWPORT_SCALE = 1024.0; // XXXdyu

float edgeDistance(vec4 p, vec4 p0, vec4 p1)
{
    return VIEWPORT_SCALE *
        abs((p.x - p0.x) * (p1.y - p0.y) -
            (p.y - p0.y) * (p1.x - p0.x)) / length(p1.xy - p0.xy);
}

void emit(int index, vec3 point, vec3 normal, vec4 edgeVerts[EDGE_VERTS])
{
    outpt.edgeDistance[0] =
        edgeDistance(edgeVerts[index], edgeVerts[0], edgeVerts[1]);
    outpt.edgeDistance[1] =
        edgeDistance(edgeVerts[index], edgeVerts[1], edgeVerts[2]);
#ifdef PRIM_TRI
    outpt.edgeDistance[2] =
        edgeDistance(edgeVerts[index], edgeVerts[2], edgeVerts[0]);
#endif
#ifdef PRIM_QUAD
    outpt.edgeDistance[2] =
        edgeDistance(edgeVerts[index], edgeVerts[2], edgeVerts[3]);
    outpt.edgeDistance[3] =
        edgeDistance(edgeVerts[index], edgeVerts[3], edgeVerts[0]);
#endif

    emit(index, point, normal);
}
#endif

void main()
{
    gl_PrimitiveID = gl_PrimitiveIDIn;

    vec3 P[3];
    for (int i = 0; i < 3; ++i) {
        P[i] = inpt[i].v.position.xyz + GetDisplacement(inpt[i].v.patchCoord)
            * displaceScale * inpt[i].v.normal;
    }

    vec3 A = (P[2] - P[0]).xyz;
    vec3 B = (P[1] - P[0]).xyz;
    vec3 n0 = normalize(cross(B, A));

#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    vec4 edgeVerts[EDGE_VERTS];
    edgeVerts[0] = ProjectionMatrix * inpt[0].v.position;
    edgeVerts[1] = ProjectionMatrix * inpt[1].v.position;
    edgeVerts[2] = ProjectionMatrix * inpt[2].v.position;

    edgeVerts[0].xy /= edgeVerts[0].w;
    edgeVerts[1].xy /= edgeVerts[1].w;
    edgeVerts[2].xy /= edgeVerts[2].w;

    emit(0, P[0], n0, edgeVerts);
    emit(1, P[1], n0, edgeVerts);
    emit(2, P[2], n0, edgeVerts);
#else
    emit(0, P[0], n0);
    emit(1, P[1], n0);
    emit(2, P[2], n0);
#endif

    EndPrimitive();
}

#endif

//--------------------------------------------------------------
// Fragment Shader
//--------------------------------------------------------------
#ifdef FRAGMENT_SHADER

in block {
    OutputVertex v;
    noperspective in vec4 edgeDistance;
} inpt;

out vec4 outColor;

#define NUM_LIGHTS 2

struct LightSource {
    vec4 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

layout(std140) uniform Lighting {
    LightSource lightSource[NUM_LIGHTS];
};

uniform vec4 diffuseColor = vec4(1);
uniform vec4 ambientColor = vec4(1);


uniform int selectedFace;
uniform int selectedIndex;
uniform float displaceScale;

vec4
lighting(vec4 diffuse, vec3 Peye, vec3 Neye)
{
    vec4 color = vec4(0);

    for (int i = 0; i < NUM_LIGHTS; ++i) {

        vec4 Plight = lightSource[i].position;

        vec3 l = (Plight.w == 0.0)
                    ? normalize(Plight.xyz) : normalize(Plight.xyz - Peye);

        vec3 n = normalize(Neye);
        vec3 h = normalize(l + vec3(0,0,1));    // directional viewer

        float d = max(0.0, dot(n, l));
        float s = pow(max(0.0, dot(n, h)), 500.0f);

        color += lightSource[i].ambient * ambientColor
            + d * lightSource[i].diffuse * diffuse
            + s * lightSource[i].specular;
    }

    color.a = 1;
    return color;
}

vec4
edgeColor(vec4 Cfill, vec4 edgeDistance)
{
#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
#ifdef PRIM_TRI
    float d =
        min(inpt.edgeDistance[0], min(inpt.edgeDistance[1], inpt.edgeDistance[2]));
#endif
#ifdef PRIM_QUAD
    float d =
        min(min(inpt.edgeDistance[0], inpt.edgeDistance[1]),
            min(inpt.edgeDistance[2], inpt.edgeDistance[3]));
#endif
    vec4 Cedge = vec4(1.0, 1.0, 0.0, 1.0);
    float p = exp2(-2 * d * d);

#if defined(GEOMETRY_OUT_WIRE)
    if (p < 0.25) discard;
#endif

    Cfill.rgb = mix(Cfill.rgb, Cedge.rgb, p);
#endif
    return Cfill;
}

#if defined(PRIM_QUAD) || defined(PRIM_TRI)
void
main()
{
#ifdef SELECTION
    vec4 color = vec4(1);
    color.r = (int)(inpt.v.patchCoord.w)/255.0;

    return;
#endif


#if 0
    vec4 du, dv;
    float disp = GetDisplacement(du, dv, inpt.v.patchCoord);
    disp *= displaceScale;
    du *= displaceScale;
    dv *= displaceScale;
    vec3 n = normalize(cross(inpt.v.tangent, inpt.v.bitangent));
    vec3 tangent = inpt.v.tangent + n * du.x;
    vec3 bitangent = inpt.v.bitangent + n * dv.x;
    vec3 normal = normalize(cross(tangent, bitangent));
#else
    vec3 normal = inpt.v.normal;
#endif
    vec3 N = (gl_FrontFacing ? normal : -normal);

#if 1
    vec4 color = diffuseColor;
#else
    vec4 color = PtexLookupNearest(inpt.v.patchCoord,
                                   hdispLevel,
                                   textureImage_Data,
                                   textureImage_Packing);
#endif

    // mix highlight
    if (int(inpt.v.patchCoord.w) == selectedFace) {
        color = mix(vec4(1,0,0,1), color, 0.8);
    }


    vec4 Cf = lighting(color, inpt.v.position.xyz, N);

#if defined(GEOMETRY_OUT_WIRE) || defined(GEOMETRY_OUT_LINE)
    Cf = edgeColor(Cf, inpt.edgeDistance);
#endif

    outColor = Cf;
}
#endif

#endif
