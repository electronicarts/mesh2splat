///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "GaussiansPrepass.hpp"

void GaussiansPrepass::execute(RenderContext& renderContext)
{

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_PREPASS, -1, "GAUSSIAN_SPLATTING_PREPASS");
#endif 
    GLuint computeShaderGaussianPrepassProgramID = renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::PrepassFiltering3dgsProgram);

    glUseProgram(computeShaderGaussianPrepassProgramID);

    float std_dev = (renderContext.gaussianStd / (float(renderContext.resolutionTarget)));

    glUtils::setUniform1f(computeShaderGaussianPrepassProgramID,     "u_stdDev", std_dev);
    glUtils::setUniformMat4(computeShaderGaussianPrepassProgramID,   "u_worldToView", renderContext.viewMat);
    glUtils::setUniformMat4(computeShaderGaussianPrepassProgramID,   "u_viewToClip", renderContext.projMat);
    glUtils::setUniform2f(computeShaderGaussianPrepassProgramID,     "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform1i(computeShaderGaussianPrepassProgramID,     "u_renderMode", renderContext.renderMode);
    glUtils::setUniform1ui(computeShaderGaussianPrepassProgramID,    "u_format", renderContext.format);
    glUtils::setUniformMat4(computeShaderGaussianPrepassProgramID,   "u_modelToWorld", renderContext.modelMat);
    glUtils::setUniform1i(computeShaderGaussianPrepassProgramID,     "u_gaussianCount", renderContext.numberOfGaussians);
    glUtils::setUniform2f(computeShaderGaussianPrepassProgramID,     "u_nearFar", glm::vec2(renderContext.nearPlane, renderContext.farPlane));
    
    glUtils::setUniform1ui(computeShaderGaussianPrepassProgramID,     "u_depthTestMesh", renderContext.performMeshDepthTest);

    glUtils::setTexture2D(computeShaderGaussianPrepassProgramID, "u_depthTexture", renderContext.meshDepthTexture, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.gaussianDepthPostFiltering);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.perQuadTransformationsBuffer);
    
    glUtils::resetAtomicCounter(renderContext.atomicCounterBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, renderContext.atomicCounterBuffer);
    
    unsigned int totalInvocations = renderContext.numberOfGaussians;

    unsigned int threadsPerGroup = 256;
    unsigned int totalGroupsNeeded = (totalInvocations + threadsPerGroup - 1) / threadsPerGroup;
    unsigned int groupsX = (unsigned int)ceil(sqrt((float)totalGroupsNeeded));
    unsigned int groupsY = (unsigned int)ceil((totalGroupsNeeded + groupsX - 1) / std::max(float(groupsX), 1.0f));
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}
