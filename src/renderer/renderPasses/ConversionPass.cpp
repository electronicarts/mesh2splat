#pragma once
#include "ConversionPass.hpp"

void ConversionPass::execute(RenderContext &renderContext)
{
    
    //TODO: If model has many meshes this is probably not the most efficient approach.
    //For how the mesh2splat method currently works, we still need to generate a separate frame and drawbuffer per mesh, but the gpu conversion
    //could be done via batch rendering (I guess (?))
    //If we are running the conversion pass means the currently existing framebuffer with respective draw buffers should be deleted before the conversion passes
    for (auto& mesh : renderContext.dataMeshAndGlMesh) {
        Mesh meshData = std::get<0>(mesh);
        GLMesh meshGl = std::get<1>(mesh);

        GLuint framebuffer;
        GLuint* drawBuffers = glUtils::setupFrameBuffer(framebuffer, renderContext.resolutionTarget, renderContext.resolutionTarget);

        glViewport(0, 0, renderContext.resolutionTarget, renderContext.resolutionTarget);
        


        //Need to do this to free memory, this means we need to rebind the gaussianBuffer in later stages
        if (renderContext.gaussianBuffer == 0) {
            glGenBuffers(1, &(renderContext.gaussianBuffer));
        }

        //glGenBuffers(1, &(renderContext.gaussianBuffer));
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);
        //TODO: I will categorize this hardcoding issue of the number of output float4 params from the SSBO as: ISSUE6
        GLsizeiptr bufferSize = renderContext.resolutionTarget * renderContext.resolutionTarget * sizeof(glm::vec4) * 6;
        GLint currentSize;
        glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &currentSize);
        if (currentSize != bufferSize) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
        }
        //unsigned int bindingPos = 5; //TODO: SSBO binding pos, should not hardcode it and should depend on how many drawbuffers from the framebuffer I want to read from
        //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPos, *gaussianBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        conversion(
            renderContext.shaderPrograms.converterShaderProgram, meshGl.vao,
            framebuffer, meshGl.vertexCount,
            renderContext.normalizedUvSpaceWidth, renderContext.normalizedUvSpaceHeight,
            renderContext.textureTypeMap, meshData.material, renderContext.resolutionTarget, renderContext.gaussianStd,
            renderContext.gaussianBuffer, renderContext.atomicCounterBuffer
        );
        glFinish();
        //glUtils::setupGaussianBufferSsbo(renderContext.resolutionTarget, renderContext.resolutionTarget, &(renderContext.gaussianBuffer));
        
        //if (renderContext.drawIndirectBuffer != 0) {
        //    glDeleteBuffers(1, &(renderContext.drawIndirectBuffer));
        //}

        //TODO: ideally this should not be necessary, and we should directly atomically append into the SSBO from the fragment shader
        // not doing so results in wasted work (wherever texture map has no data), but need to handle fragment syncronization. For now this is ok.
            
        //aggregation(renderContext.shaderPrograms.computeShaderProgram, drawBuffers, renderContext.gaussianBuffer, renderContext.drawIndirectBuffer, renderContext.atomicCounterBuffer, renderContext.resolutionTarget);

        const int numberOfTextures = 5;
        glDeleteTextures(numberOfTextures, drawBuffers); 
        delete[] drawBuffers;  //TODO: honestly probably not necessary                          
        glDeleteFramebuffers(1, &framebuffer);
    }
}


void ConversionPass::conversion(
        GLuint shaderProgram, GLuint vao,
        GLuint framebuffer, size_t vertexCount,
        int normalizedUVSpaceWidth, int normalizedUVSpaceHeight,
        const std::map<std::string, TextureDataGl>& textureTypeMap, MaterialGltf material, unsigned int referenceResolution, float GAUSSIAN_STD,
        GLuint gaussianBuffer, GLuint atomicCounter
    ) 
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::CONVERSION_PASS, -1, "CONVERSION_VS_GS_PS_PASS");
#endif 
    // Use shader program and perform tessellation
    glUseProgram(shaderProgram);

    //-------------------------------SET UNIFORMS-------------------------------   
    // TODO: these should be set, but by setting them here it would influence the rendering part which edits the std dev 
    // FIX: these are not set here but in the gaussiansplatting compute prepass, conditionally if it is loaded from .ply or mesh based on pbr.w which acts as flag
    glUtils::setUniform1f(shaderProgram, "u_sigma_x", GAUSSIAN_STD / (float(referenceResolution))); //I separate x and y in case want it to be anisotropic in the future
    glUtils::setUniform1f(shaderProgram, "u_sigma_y", GAUSSIAN_STD / (float(referenceResolution)));

    //Textures
    if (textureTypeMap.find(BASE_COLOR_TEXTURE) != textureTypeMap.end())
    {
        glUtils::setTexture2D(shaderProgram, "albedoTexture", textureTypeMap.at(BASE_COLOR_TEXTURE).glTextureID,     0);
        glUtils::setUniform1i(shaderProgram, "hasAlbedoMap", 1);
    }
    if (textureTypeMap.find(NORMAL_TEXTURE) != textureTypeMap.end())
    {
        glUtils::setTexture2D(shaderProgram, "normalTexture", textureTypeMap.at(NORMAL_TEXTURE).glTextureID,         1);
        glUtils::setUniform1i(shaderProgram, "hasNormalMap", 1);
    }
    if (textureTypeMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureTypeMap.end())
    {
        glUtils::setTexture2D(shaderProgram, "metallicRoughnessTexture", textureTypeMap.at(METALLIC_ROUGHNESS_TEXTURE).glTextureID,     2);
        glUtils::setUniform1i(shaderProgram, "hasMetallicRoughnessMap", 1);
    }
    if (textureTypeMap.find(AO_TEXTURE) != textureTypeMap.end())
    {
        glUtils::setTexture2D(shaderProgram, "occlusionTexture", textureTypeMap.at(AO_TEXTURE).glTextureID,          3);
    }
    if (textureTypeMap.find(EMISSIVE_TEXTURE) != textureTypeMap.end())
    {
        glUtils::setTexture2D(shaderProgram, "emissiveTexture", textureTypeMap.at(EMISSIVE_TEXTURE).glTextureID,     4);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gaussianBuffer);
    
    glUtils::resetAtomicCounter(atomicCounter);
    glFinish();
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, atomicCounter);
    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glDrawArrays(GL_TRIANGLES, 0, vertexCount); 

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
 
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void ConversionPass::aggregation(GLuint& computeShaderProgram, GLuint* drawBuffers, GLuint& gaussianBuffer, GLuint& drawIndirectBuffer, GLuint& atomicCounterBuffer, unsigned int resolutionTarget)
{

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::CONVERSION_AGGREGATION_PASS, -1, "CONVERSION_AGGREGATION_PASS");
#endif 

    glUseProgram(computeShaderProgram);

    unsigned int i = 0;
    for (auto uniformName : std::vector<std::string>{ "texPositionAndScaleX", "scaleZAndNormal", "rotationAsQuat", "texColor", "pbrAndScaleY" })
    {
        glUtils::setTexture2D(computeShaderProgram, uniformName,    drawBuffers[i], i);
        ++i;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gaussianBuffer);

    glUtils::resetAtomicCounter(atomicCounterBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 6, atomicCounterBuffer);

    GLuint groupsX = (GLuint)ceil(resolutionTarget / 16.0);
    GLuint groupsY = (GLuint)ceil(resolutionTarget / 16.0);

    glDispatchCompute(groupsX, groupsY, 1);

    // Ensure compute shader completion
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 

}
