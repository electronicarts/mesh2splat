#include "mesh2SplatHandler.hpp"


void Mesh2splatConverterHandler::runComputePass(GLuint& computeShaderProgram, GLuint* drawBuffers, GLuint& gaussianBuffer, GLuint& drawIndirectBuffer, unsigned int resolutionTarget)
{
    glGenBuffers(1, &drawIndirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

    glUseProgram(computeShaderProgram);

    setTexture2D(computeShaderProgram, "texPositionAndScaleX",    drawBuffers[0], 0);
    setTexture2D(computeShaderProgram, "scaleZAndNormal",         drawBuffers[1], 1);
    setTexture2D(computeShaderProgram, "rotationAsQuat",          drawBuffers[2], 2);
    setTexture2D(computeShaderProgram, "texColor",                drawBuffers[3], 3);
    setTexture2D(computeShaderProgram, "pbrAndScaleY",            drawBuffers[4], 4);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gaussianBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawIndirectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, drawIndirectBuffer);

    // Dispatch compute work groups
    GLuint groupsX = (GLuint)ceil(resolutionTarget / 16.0);
    GLuint groupsY = (GLuint)ceil(resolutionTarget / 16.0);

    glDispatchCompute(groupsX, groupsY, 1);

    // Ensure compute shader completion
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}


void Mesh2splatConverterHandler::runConversionPass(const std::string& meshFilePath, const std::string& baseFolder,
                   int resolution, float gaussianStd,
                   GLuint converterShaderProgram, GLuint computeShaderProgram,
                   std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh,
                   int normalizedUvSpaceWidth, int normalizedUvSpaceHeight,
                   std::map<std::string, TextureDataGl>& textureTypeMap,
                   GLuint& gaussianBuffer, GLuint &drawIndirectBuffer) 
{
    //TODO: If model has many meshes this is probably not the most efficient approach.
    //For how the mesh2splat method currently works, we still need to generate a separate frame and drawbuffer per mesh, but the gpu conversion
    //could be done via batch rendering (I guess (?))

    for (auto& mesh : dataMeshAndGlMesh) {
        Mesh meshData = std::get<0>(mesh);
        GLMesh meshGl = std::get<1>(mesh);

        GLuint framebuffer;
        GLuint* drawBuffers = setupFrameBuffer(framebuffer, resolution, resolution);

        glViewport(0, 0, resolution, resolution); //Set this as gpu conversion is dependant on viewport
        
        performGpuConversion(
            converterShaderProgram, meshGl.vao,
            framebuffer, meshGl.vertexCount,
            normalizedUvSpaceWidth, normalizedUvSpaceHeight,
            textureTypeMap, meshData.material, resolution, gaussianStd
        );

        //Need to do this to free memory, this means we need to rebind the gaussianBuffer in later stages
        if (gaussianBuffer != 0) {
            glDeleteBuffers(1, &gaussianBuffer);
        }

        setupSsbo(resolution, resolution, &gaussianBuffer);
        
        if (drawIndirectBuffer != 0) {
            glDeleteBuffers(1, &drawIndirectBuffer);
        }

        //TODO: ideally this should not be necessary, and we should directly atomically append into the SSBO from the fragment shader
        // not doing so results in wasted work (wherever texture map has no data), but need to handle fragment syncronization. For now this is ok.
        runComputePass(computeShaderProgram, drawBuffers, gaussianBuffer, drawIndirectBuffer, resolution);

        // Cleanup framebuffer and drawbuffers
        const int numberOfTextures = 5;
        glDeleteTextures(numberOfTextures, drawBuffers); 
        delete[] drawBuffers;                            
        glDeleteFramebuffers(1, &framebuffer);
    }

}

void Mesh2splatConverterHandler::performGpuConversion(
    GLuint shaderProgram, GLuint vao,
    GLuint framebuffer, size_t vertexCount,
    int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
    const std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material, unsigned int referenceResolution, float GAUSSIAN_STD
) {
    // Use shader program and perform tessellation
    glUseProgram(shaderProgram);

    //-------------------------------SET UNIFORMS-------------------------------   
    setUniform1f(shaderProgram, "u_sigma_x", GAUSSIAN_STD / (float(referenceResolution))); //I separate x and y in case want it to be anisotropic in the future
    setUniform1f(shaderProgram, "u_sigma_y", GAUSSIAN_STD / (float(referenceResolution)));

    //Textures
    if (textureTypeMap.find(BASE_COLOR_TEXTURE) != textureTypeMap.end())
    {
        setTexture2D(shaderProgram, "albedoTexture", textureTypeMap.at(BASE_COLOR_TEXTURE).glTextureID,     0);
        setUniform1i(shaderProgram, "hasAlbedoMap", 1);
    }
    if (textureTypeMap.find(NORMAL_TEXTURE) != textureTypeMap.end())
    {
        setTexture2D(shaderProgram, "normalTexture", textureTypeMap.at(NORMAL_TEXTURE).glTextureID,         1);
        setUniform1i(shaderProgram, "hasNormalMap", 1);
    }
    if (textureTypeMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureTypeMap.end())
    {
        setTexture2D(shaderProgram, "metallicRoughnessTexture", textureTypeMap.at(METALLIC_ROUGHNESS_TEXTURE).glTextureID,     2);
        setUniform1i(shaderProgram, "hasMetallicRoughnessMap", 1);
    }
    if (textureTypeMap.find(AO_TEXTURE) != textureTypeMap.end())
    {
        setTexture2D(shaderProgram, "occlusionTexture", textureTypeMap.at(AO_TEXTURE).glTextureID,          3);
    }
    if (textureTypeMap.find(EMISSIVE_TEXTURE) != textureTypeMap.end())
    {
        setTexture2D(shaderProgram, "emissiveTexture", textureTypeMap.at(EMISSIVE_TEXTURE).glTextureID,     4);
    }

    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glDrawArrays(GL_TRIANGLES, 0, vertexCount); 

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Mesh2splatConverterHandler::prepareMeshAndUploadToGPU(std::string filename, std::string base_folder, std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh, int& uvSpaceWidth, int& uvSpaceHeight)
{
    std::vector<Mesh> meshes;
    parseGltfFileToMesh(filename, base_folder, meshes);
    printf("Parsed gltf mesh file\n");

    printf("Generating normalized UV coordinates (XATLAS)\n");
    generateNormalizedUvCoordinatesPerFace(uvSpaceWidth, uvSpaceHeight, meshes);
    
    printf("Loading mesh into OPENGL buffers\n");
    generateMeshesVBO(meshes, dataMeshAndGlMesh);
}
