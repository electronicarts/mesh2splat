///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

// runs the offscreen conversion pass that rasterizes meshes into Gaussian SSBO data.

#pragma once
#include "ConversionPass.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

void ConversionPass::execute(RenderContext &renderContext)
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::CONVERSION_PASS, -1, "CONVERSION_VS_GS_PS_PASS");
#endif

    GLuint converterProgramID = renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::ConverterProgram);
    glUseProgram(converterProgramID);

    renderContext.numberOfGaussians = 0;

    const uint32_t requestedRes = std::max(1u, renderContext.resolutionTarget);

    auto runPass = [&](uint32_t res, bool countOnly, float sampleProb, uint32_t seed, uint32_t maxSplats) -> uint32_t {
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.atomicCounterBufferConversionPass);
        uint32_t zero = 0;
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &zero);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

        uint32_t zeros[5] = {};
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.conversionDebugCounters);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(zeros), zeros, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, renderContext.conversionDebugCounters);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
        if (renderContext.debugColorStats && renderContext.debugPrimIdBuffer != 0) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, renderContext.debugPrimIdBuffer);
        }
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, renderContext.atomicCounterBufferConversionPass);

        glUtils::setUniform1i(converterProgramID, "u_countOnly", countOnly ? 1 : 0);
        glUtils::setUniform1f(converterProgramID, "u_sampleProb", sampleProb);
        glUtils::setUniform1ui(converterProgramID, "u_hashSeed", seed);
        glUtils::setUniform1ui(converterProgramID, "u_maxSplats", maxSplats);
        glUtils::setUniform1i(converterProgramID, "u_writePrimId", renderContext.debugColorStats ? 1 : 0);

        GLuint framebuffer;
        GLuint drawBuffers = glUtils::setupFrameBuffer(framebuffer, res, res);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, (GLsizei)res, (GLsizei)res);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        for (auto& mesh : renderContext.dataMeshAndGlMesh) {
            conversion(renderContext, mesh, framebuffer);
        }

        glFinish();
        glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.atomicCounterBufferConversionPass);
        uint32_t numGs = 0;
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &numGs);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

        uint32_t countersLocal[5] = {};
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.conversionDebugCounters);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(countersLocal), countersLocal);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &drawBuffers);
        glDeleteFramebuffers(1, &framebuffer);

        if (countOnly) {
            return countersLocal[0];
        }
        return numGs;
    };

    static uint32_t seedCounter = 1;
    const uint32_t seed = seedCounter++;

    uint32_t candidatesRequested = runPass(requestedRes, true, 1.0f, seed, 1u);
    const uint32_t maxSplats = std::max(1u, candidatesRequested);
    const uint32_t effectiveRes = requestedRes;
    const char* strategy = "none";
    const float sampleProb = 1.0f;

    GLsizeiptr bufferSize = static_cast<GLsizeiptr>(maxSplats) * sizeof(utils::GaussianDataSSBO);
    GLint currentSize = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &currentSize);
    if (currentSize != bufferSize) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (renderContext.debugColorStats) {
        if (renderContext.debugPrimIdBuffer == 0) {
            glGenBuffers(1, &renderContext.debugPrimIdBuffer);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.debugPrimIdBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(maxSplats) * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    uint32_t numGs = runPass(effectiveRes, false, sampleProb, seed + 1, maxSplats);

    uint32_t counters[5] = {};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.conversionDebugCounters);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(counters), counters);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    uint32_t written = counters[3];
    float overflow = 0.0f;

    std::cout << "[mesh2splat] ConversionPass: candidates=" << candidatesRequested
              << " maxSplats=" << maxSplats
              << " kept=" << written
              << " overflow=" << overflow
              << " sampleProb=" << sampleProb
              << " res=" << effectiveRes
              << " strategy=" << strategy
              << " bufferSize=" << bufferSize
              << " currentSize=" << currentSize
              << std::endl;

    std::cout << "[mesh2splat] ConversionCounters: candidates=" << counters[0]
              << " accepted=" << counters[1]
              << " attemptedWrites=" << counters[2]
              << " written=" << counters[3]
              << " oobRejected=" << counters[4]
              << " maxSplats=" << maxSplats
              << " atomicCount=" << numGs
              << std::endl;

    renderContext.numberOfGaussians = written;
    if (candidatesRequested == 0) {
        std::cerr << "ERROR: No fragments rendered in conversion pass. Check transforms, bounds, and culling." << std::endl;
    }

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
            glUtils::setUniform1i(converterProgramID, "hasAlbedoMap", 1);
        }
        else
        {
            glUtils::setUniform1i(converterProgramID, "hasAlbedoMap", 0);
        }

        if (textureMap.find(NORMAL_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "normalTexture", textureMap.at(NORMAL_TEXTURE).glTextureID, 1);
            glUtils::setUniform1i(converterProgramID, "hasNormalMap", 1);
        }
        else
        {
            glUtils::setUniform1i(converterProgramID, "hasNormalMap", 0);
        }

        if (textureMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "metallicRoughnessTexture", textureMap.at(METALLIC_ROUGHNESS_TEXTURE).glTextureID, 2);
            glUtils::setUniform1i(converterProgramID, "hasMetallicRoughnessMap", 1);
        }
        else
        {
            glUtils::setUniform1i(converterProgramID, "hasMetallicRoughnessMap", 0);
        }

        if (textureMap.find(AO_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "occlusionTexture", textureMap.at(AO_TEXTURE).glTextureID, 3);
        }

        if (textureMap.find(EMISSIVE_TEXTURE) != textureMap.end())
        {
            glUtils::setTexture2D(converterProgramID, "emissiveTexture", textureMap.at(EMISSIVE_TEXTURE).glTextureID, 4);
        }
    }
    else
    {
        glUtils::setUniform1i(converterProgramID, "hasAlbedoMap", 0);
        glUtils::setUniform1i(converterProgramID, "hasNormalMap", 0);
        glUtils::setUniform1i(converterProgramID, "hasMetallicRoughnessMap", 0);
    }

    glUtils::setUniform4f(converterProgramID, "u_materialFactor", mesh.first.material.baseColorFactor);
    glUtils::setUniform3f(converterProgramID, "u_bboxMin", mesh.first.bbox.min);
    glUtils::setUniform3f(converterProgramID, "u_bboxMax", mesh.first.bbox.max);
    if (renderContext.debugColorStats) {
        glUtils::setUniform1ui(converterProgramID, "u_debugPrimId", static_cast<unsigned int>(mesh.first.primitiveIndex));
    }

    glBindVertexArray(mesh.second.vao);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mesh.second.vertexCount);
}
