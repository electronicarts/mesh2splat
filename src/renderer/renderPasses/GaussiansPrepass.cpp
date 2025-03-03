#include "GaussiansPrepass.hpp"

void GaussiansPrepass::execute(RenderContext& renderContext)
{

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_PREPASS, -1, "GAUSSIAN_SPLATTING_PREPASS");
#endif 
    glUseProgram(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram);

    float std_dev = (renderContext.gaussianStd / (float(renderContext.resolutionTarget)));

    glUtils::setUniform1f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_stdDev", std_dev);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,   "u_worldToView", renderContext.viewMat);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,   "u_viewToClip", renderContext.projMat);
    glUtils::setUniform2f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform1i(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_renderMode", renderContext.renderMode);
    glUtils::setUniform1ui(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,    "u_format", renderContext.format);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,   "u_modelToWorld", renderContext.modelMat);
    glUtils::setUniform1i(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_gaussianCount", renderContext.numberOfGaussians);
    glUtils::setUniform2f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_nearFar", glm::vec2(renderContext.nearPlane, renderContext.farPlane));
           
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.gaussianDepthPostFiltering);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.perQuadTransformationsBuffer);
    
    glUtils::resetAtomicCounter(renderContext.atomicCounterBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, renderContext.atomicCounterBuffer);
    
    unsigned int totalInvocations = renderContext.numberOfGaussians;

    unsigned int threadsPerGroup = 256;
    unsigned int totalGroupsNeeded = (totalInvocations + threadsPerGroup - 1) / threadsPerGroup;
    unsigned int groupsX = (unsigned int)ceil(sqrt((float)totalGroupsNeeded));
    unsigned int groupsY = (totalGroupsNeeded + groupsX - 1) / groupsX; 
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}
