#include "mesh2SplatHandler.hpp"

GLuint Mesh2splatConverterHandler::compute_shader_dispatch(GLuint computeShaderProgram, GLuint* drawBuffers, GLuint gaussianBuffer, unsigned int resolutionTarget)
{
    GLuint drawIndirectBuffer;
    glGenBuffers(1, &drawIndirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

    glUseProgram(computeShaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[0]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "texPositionAndScaleX"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[1]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "scaleZAndNormal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[2]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "rotationAsQuat"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[3]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "texColor"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[4]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "pbrAndScaleY"), 4);


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
    glFinish(); 
    return drawIndirectBuffer;
}


void Mesh2splatConverterHandler::runConversion(const std::string& meshFilePath, const std::string& baseFolder,
                   int resolution, float gaussianStd,
                   GLuint converterShaderProgram, GLuint computeShaderProgram,
                   std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh,
                   int normalizedUvSpaceWidth, int normalizedUvSpaceHeight,
                   std::map<std::string, TextureDataGl>& textureTypeMap,
                   GLuint& gaussianBuffer, GLuint &drawIndirectBuffer) 
{
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

        if (gaussianBuffer != 0) {
            glDeleteBuffers(1, &gaussianBuffer);
        }
        setupSsbo(resolution, resolution, &gaussianBuffer);
        
        if (drawIndirectBuffer != 0) {
            glDeleteBuffers(1, &drawIndirectBuffer);
        }
        //TODO: ideally this should not be necessary, and we should directly atomically append into the SSBO from the fragment shader
        // not doing so results in wasted work, but need to handle fragment syncronization. For now this is ok.
        //TODO: refactor, pass drawIndirectBuffer as argument not return
        drawIndirectBuffer = compute_shader_dispatch(computeShaderProgram, drawBuffers, gaussianBuffer, resolution);

        // Cleanup framebuffer and drawbuffers
        const int numberOfTextures = 5;
        glDeleteTextures(numberOfTextures, drawBuffers); 
        glDeleteFramebuffers(1, &framebuffer);         
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
    setUniform3f(shaderProgram, "meshMaterialColor", material.baseColorFactor);
    setUniform1f(shaderProgram, "sigma_x", GAUSSIAN_STD / (float(referenceResolution))); //I separate x and y in case want it to be anisotropic in the future
    setUniform1f(shaderProgram, "sigma_y", GAUSSIAN_STD / (float(referenceResolution)));

    //Textures
    if (textureTypeMap.find(BASE_COLOR_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "albedoTexture", textureTypeMap.at(BASE_COLOR_TEXTURE).glTextureID,                        0);
        setUniform1i(shaderProgram, "hasAlbedoMap", 1);
    }
    if (textureTypeMap.find(NORMAL_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "normalTexture", textureTypeMap.at(NORMAL_TEXTURE).glTextureID,                            1);
        setUniform1i(shaderProgram, "hasNormalMap", 1);
    }
    if (textureTypeMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "metallicRoughnessTexture", textureTypeMap.at(METALLIC_ROUGHNESS_TEXTURE).glTextureID,     2);
        setUniform1i(shaderProgram, "hasMetallicRoughnessMap", 1);
    }
    if (textureTypeMap.find(AO_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "occlusionTexture", textureTypeMap.at(AO_TEXTURE).glTextureID,                      3);
    }
    if (textureTypeMap.find(EMISSIVE_TEXTURE) != textureTypeMap.end())
    {
        setTexture(shaderProgram, "emissiveTexture", textureTypeMap.at(EMISSIVE_TEXTURE).glTextureID,                        4);
    }

    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glDrawArrays(GL_TRIANGLES, 0, vertexCount); 

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Mesh2splatConverterHandler::prepareMeshAndUploadToGPU(std::string filename, std::string base_folder, std::vector<std::pair<Mesh, GLMesh>>& dataMeshAndGlMesh, int& uvSpaceWidth, int& uvSpaceHeight)
{
    std::vector<Mesh> meshes = parseGltfFileToMesh(filename, base_folder);

    printf("Parsed gltf mesh file\n");

    printf("Generating normalized UV coordinates (XATLAS)\n");
    generateNormalizedUvCoordinatesPerFace(uvSpaceWidth, uvSpaceHeight, meshes);

    printf("Loading mesh into OPENGL buffers\n");
    uploadMeshesToOpenGL(meshes, dataMeshAndGlMesh);
}
