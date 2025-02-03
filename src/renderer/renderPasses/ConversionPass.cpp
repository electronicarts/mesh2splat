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
        GLuint drawBuffers = glUtils::setupFrameBuffer(framebuffer, renderContext.resolutionTarget, renderContext.resolutionTarget);

        glViewport(0, 0, renderContext.resolutionTarget, renderContext.resolutionTarget);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);
        //TODO: I will categorize this hardcoding issue of the number of output float4 params from the SSBO as: ISSUE6
        GLsizeiptr bufferSize = renderContext.resolutionTarget * renderContext.resolutionTarget * sizeof(glm::vec4) * 6;
        GLint currentSize;
        glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &currentSize);
        if (currentSize != bufferSize) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        conversion(renderContext, mesh, framebuffer);

        glFinish();

        //Read number of elements
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.atomicCounterBufferConversionPass);
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &renderContext.numberOfGaussians);
        glFinish();

        glDeleteRenderbuffers(1, &drawBuffers); 

        glDeleteFramebuffers(1, &framebuffer);
    }
}


void ConversionPass::conversion(
        RenderContext renderContext, std::pair<Mesh, GLMesh> mesh, GLuint dummyFramebuffer
    ) 
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::CONVERSION_PASS, -1, "CONVERSION_VS_GS_PS_PASS");
#endif 
    // Use shader program and perform tessellation
    glUseProgram(renderContext.shaderPrograms.converterShaderProgram);

    //-------------------------------SET UNIFORMS-------------------------------   
    //Textures
    if (renderContext.textureTypeMap.find(BASE_COLOR_TEXTURE) != renderContext.textureTypeMap.end())
    {
        glUtils::setTexture2D(renderContext.shaderPrograms.converterShaderProgram, "albedoTexture", renderContext.textureTypeMap.at(BASE_COLOR_TEXTURE).glTextureID,     0);
        glUtils::setUniform1i(renderContext.shaderPrograms.converterShaderProgram, "hasAlbedoMap", 1);
    }
    if (renderContext.textureTypeMap.find(NORMAL_TEXTURE) != renderContext.textureTypeMap.end())
    {
        glUtils::setTexture2D(renderContext.shaderPrograms.converterShaderProgram, "normalTexture", renderContext.textureTypeMap.at(NORMAL_TEXTURE).glTextureID,         1);
        glUtils::setUniform1i(renderContext.shaderPrograms.converterShaderProgram, "hasNormalMap", 1);
    }
    if (renderContext.textureTypeMap.find(METALLIC_ROUGHNESS_TEXTURE) != renderContext.textureTypeMap.end())
    {
        glUtils::setTexture2D(renderContext.shaderPrograms.converterShaderProgram, "metallicRoughnessTexture", renderContext.textureTypeMap.at(METALLIC_ROUGHNESS_TEXTURE).glTextureID,     2);
        glUtils::setUniform1i(renderContext.shaderPrograms.converterShaderProgram, "hasMetallicRoughnessMap", 1);
    }
    if (renderContext.textureTypeMap.find(AO_TEXTURE) != renderContext.textureTypeMap.end())
    {
        glUtils::setTexture2D(renderContext.shaderPrograms.converterShaderProgram, "occlusionTexture", renderContext.textureTypeMap.at(AO_TEXTURE).glTextureID,          3);
    }
    if (renderContext.textureTypeMap.find(EMISSIVE_TEXTURE) != renderContext.textureTypeMap.end())
    {
        glUtils::setTexture2D(renderContext.shaderPrograms.converterShaderProgram, "emissiveTexture", renderContext.textureTypeMap.at(EMISSIVE_TEXTURE).glTextureID,     4);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.shaderPrograms.converterShaderProgram);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
    
    glUtils::resetAtomicCounter(renderContext.atomicCounterBufferConversionPass);
    glFinish();
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, renderContext.atomicCounterBufferConversionPass);
    glBindVertexArray(mesh.second.vao);
    glBindFramebuffer(GL_FRAMEBUFFER, dummyFramebuffer);

    glDrawArrays(GL_TRIANGLES, 0, mesh.second.vertexCount); 

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
 
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}