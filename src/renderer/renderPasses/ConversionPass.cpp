///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "ConversionPass.hpp"

void ConversionPass::execute(RenderContext &renderContext)
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::CONVERSION_PASS, -1, "CONVERSION_VS_GS_PS_PASS");
#endif 

    glUseProgram(renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::ConverterProgram));

    renderContext.numberOfGaussians = 0;
    glUtils::resetAtomicCounter(renderContext.atomicCounterBufferConversionPass);

    GLsizeiptr bufferSize = renderContext.resolutionTarget * renderContext.resolutionTarget * sizeof(glm::vec4) * 6 * 6; //Last *6 is just to avoid running out of space
    GLint currentSize;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);    
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &currentSize);
    
    if (currentSize != bufferSize) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
    }

    GLuint framebuffer;
    GLuint drawBuffers = glUtils::setupFrameBuffer(framebuffer, renderContext.resolutionTarget, renderContext.resolutionTarget);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);    
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, renderContext.atomicCounterBufferConversionPass);
    
    glViewport(0, 0, int(renderContext.resolutionTarget), int(renderContext.resolutionTarget));
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    for (auto& mesh : renderContext.dataMeshAndGlMesh) {
        conversion(renderContext, mesh, framebuffer);
    }

    glFinish();

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.atomicCounterBufferConversionPass);
    uint32_t numGs;
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &numGs);
    renderContext.numberOfGaussians = numGs;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteRenderbuffers(1, &drawBuffers); 
    glDeleteFramebuffers(1, &framebuffer);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}

void ConversionPass::conversion(
        RenderContext& renderContext, std::pair<utils::Mesh, utils::GLMesh>& mesh, GLuint dummyFramebuffer
    ) 
{

    GLuint converterProgramID = renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::ConverterProgram);

    if (renderContext.meshToTextureData.find(mesh.first.name) != renderContext.meshToTextureData.end())
    {
        auto& textureMap = renderContext.meshToTextureData.at(mesh.first.name);

        if (textureMap.find(BASE_COLOR_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "albedoTexture", textureMap.at(BASE_COLOR_TEXTURE).glTextureID, 0);

            glUtils::setTexture2D(converterProgramID, "albedoTexture", textureMap.at(BASE_COLOR_TEXTURE).glTextureID, 0);
            glUtils::setUniform1i(converterProgramID, "hasAlbedoMap", 1);
        }
        if (textureMap.find(NORMAL_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "normalTexture", textureMap.at(NORMAL_TEXTURE).glTextureID,         1);
            glUtils::setUniform1i(converterProgramID, "hasNormalMap", 1);
        }
        if (textureMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "metallicRoughnessTexture", textureMap.at(METALLIC_ROUGHNESS_TEXTURE).glTextureID,     2);
            glUtils::setUniform1i(converterProgramID, "hasMetallicRoughnessMap", 1);
        }
        if (textureMap.find(AO_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "occlusionTexture", textureMap.at(AO_TEXTURE).glTextureID,          3);
        }
        if (textureMap.find(EMISSIVE_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "emissiveTexture", textureMap.at(EMISSIVE_TEXTURE).glTextureID,     4);
        }
    }

    glUtils::setUniform4f(converterProgramID,      "u_materialFactor", mesh.first.material.baseColorFactor);
    glUtils::setUniform3f(converterProgramID,      "u_bboxMin", mesh.first.bbox.min);
    glUtils::setUniform3f(converterProgramID,      "u_bboxMax", mesh.first.bbox.max);

    glBindVertexArray(mesh.second.vao);

    glDrawArrays(GL_TRIANGLES, 0, mesh.second.vertexCount); 
 
}