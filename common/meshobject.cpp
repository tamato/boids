#include <stdlib.h>
#include <iostream>

#define GLEW_NO_GLU
#include <GL/glew.h>

#include "meshobject.h"

#define bufferOffest(x) ((char*)NULL+(x))

MeshObject::MeshObject()
    : IndiceCnt(0)
    , VertCnt(0)
    , EnabledArrays(0)
    , VAO(0)
    , VBO(0)
    , IBO(0)
    , PivotPoint(0,0,0)
    , AABBMin( 9e23f)
    , AABBMax(-9e23f)
    , IndexRangeStart(0)
    , IndexRangeEnd(0)
{
    /************************************************************************************
      According to:
      http://www.opengl.org/registry/specs/NV/vertex_program.txt

      vertex attribute indices are:

        Vertex
        Attribute  Conventional                                           Conventional
        Register   Per-vertex        Conventional                         Component
        Number     Parameter         Per-vertex Parameter Command         Mapping
        ---------  ---------------   -----------------------------------  ------------
         0         vertex position   Vertex                               x,y,z,w
         1         vertex weights    VertexWeightEXT                      w,0,0,1
         2         normal            Normal                               x,y,z,1
         3         primary color     Color                                r,g,b,a
         4         secondary color   SecondaryColorEXT                    r,g,b,1
         5         fog coordinate    FogCoordEXT                          fc,0,0,1
         6         -                 -                                    -
         7         -                 -                                    -
         8         texture coord 0   MultiTexCoord(GL_TEXTURE0_ARB, ...)  s,t,r,q
         9         texture coord 1   MultiTexCoord(GL_TEXTURE1_ARB, ...)  s,t,r,q
         10        texture coord 2   MultiTexCoord(GL_TEXTURE2_ARB, ...)  s,t,r,q
         11        texture coord 3   MultiTexCoord(GL_TEXTURE3_ARB, ...)  s,t,r,q
         12        texture coord 4   MultiTexCoord(GL_TEXTURE4_ARB, ...)  s,t,r,q
         13        texture coord 5   MultiTexCoord(GL_TEXTURE5_ARB, ...)  s,t,r,q
         14        texture coord 6   MultiTexCoord(GL_TEXTURE6_ARB, ...)  s,t,r,q
         15        texture coord 7   MultiTexCoord(GL_TEXTURE7_ARB, ...)  s,t,r,q

        Table X.2:  Aliasing of vertex attributes with conventional per-vertex
        parameters.
    /**************************************************************************************/
}

MeshObject::~MeshObject()
{
    for (GLuint i=0; i<EnabledArrays; ++i)
        glDisableVertexAttribArray(i);

    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &IBO);
    glDeleteBuffers(1, &IBO);
    glDeleteVertexArrays(1, &VAO);
}

void MeshObject::init(const MeshBuffer& meshObj)
{
    // setup the mesh 
    setMesh(meshObj);
}

void MeshObject::update()
{

}

void MeshObject::render()
{
    glBindVertexArray(VAO);

    if (IndiceCnt)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        //glDrawElements(GL_TRIANGLES, IndiceCnt/*-IndexRangeStart*/, GL_UNSIGNED_INT, (const void*)(IndexRangeStart * sizeof(unsigned int)));
        glDrawElements(GL_TRIANGLES, IndexRangeEnd, GL_UNSIGNED_INT, (const void*)(IndexRangeStart * sizeof(unsigned int)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    else
    {
        glDrawArrays(GL_TRIANGLES, 0, VertCnt);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshObject::setMesh(const MeshBuffer& meshBuffer)
{
    Mesh = meshBuffer;
    VertCnt = meshBuffer.getVertCnt();
    computeBoundingBox();

    unsigned int vertArrayCnt = VertCnt;
    int VertComponentCount = sizeof(glm::vec3) / sizeof(float);
    int NormalComponentCount = sizeof(glm::vec3) / sizeof(float);
    int UVComponentCount = sizeof(glm::vec2) / sizeof(float);
    Stride = VertComponentCount; // will always have positions
    NormOffset = 0;
    UvOffset = 0;
    Normalidx = 0;
    UVidx = 0;
    EnabledArrays = 1; // 1 is for positions
    if (meshBuffer.UsesNormals)
    {
        Normalidx = EnabledArrays++;
        vertArrayCnt += VertCnt;
        NormOffset = Stride;
        Stride += NormalComponentCount;
    }
    if (meshBuffer.UsesUVs)
    {
        UVidx = EnabledArrays++;
        vertArrayCnt += VertCnt;
        UvOffset = Stride;
        Stride += UVComponentCount;
    }
    if (meshBuffer.getIndices())
    {
        IndiceCnt = meshBuffer.getIdxCnt();
    }

    const float* pos = (float*)&Mesh.getVerts()[0];
    const float* norm = (float*)&Mesh.getNorms()[0];
    const float* uv = (float*)&Mesh.getTexCoords(0)[0];
    float* vertArray = new float[vertArrayCnt*Stride];
    for (int i=0, idx=0, uvidx=0; i<VertCnt; i++, idx+=4, uvidx+=2)
    {
        int vi = i*Stride;
        for (int j=0; j<VertComponentCount; ++j)
            vertArray[vi+j] = pos[idx+j];

        if (NormOffset)
        {
            int ni = i*Stride+NormOffset;
            for (int j=0; j<NormalComponentCount; ++j)
                vertArray[ni+j] = norm[idx+j];
        }

        if (UvOffset)
        {
            int ti = i*Stride+UvOffset;
            for (int j=0; j<UVComponentCount; ++j)
                vertArray[ti+j] = uv[idx+j];
        }
    }

    // make values go from number of componets to number of bytes
    Stride *= 4;
    NormOffset *= 4;
    UvOffset *= 4;

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glGenBuffers(1, &IBO);

    for (GLuint i=0; i<EnabledArrays; ++i)
        glEnableVertexAttribArray(i);

    // set vert buffer    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 
        vertArrayCnt*Stride,
        (GLvoid*)vertArray,
        GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, Stride, bufferOffest(0));

    if (Normalidx)
        glVertexAttribPointer(Normalidx, 3, GL_FLOAT, GL_FALSE, Stride, bufferOffest(NormOffset));

    if (UVidx)
        glVertexAttribPointer(UVidx, 2, GL_FLOAT, GL_FALSE, Stride, bufferOffest(UvOffset));

    delete [] vertArray;
    if (IndiceCnt)
    {        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            IndiceCnt * sizeof(GLuint),
            (GLvoid*)meshBuffer.getIndices(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        IndexRangeStart = 0;
        IndexRangeEnd = IndiceCnt;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void MeshObject::computeBoundingBox()
{
    const std::vector<glm::vec3>& verts = Mesh.getVerts();
    for (int i=0; i<(int)verts.size(); ++i)
    {
        if (verts[i][0] < AABBMin[0]) AABBMin[0] = verts[i][0];
        if (verts[i][1] < AABBMin[1]) AABBMin[1] = verts[i][1];
        if (verts[i][2] < AABBMin[2]) AABBMin[2] = verts[i][2];

        if (verts[i][0] > AABBMax[0]) AABBMax[0] = verts[i][0];
        if (verts[i][1] > AABBMax[1]) AABBMax[1] = verts[i][1];
        if (verts[i][2] > AABBMax[2]) AABBMax[2] = verts[i][2];
    }

    glm::vec3 diagonal = AABBMax - AABBMin;
    PivotPoint = AABBMin + (diagonal * 0.5f);
}

const MeshBuffer& MeshObject::getMesh()
{
    return Mesh;
}
