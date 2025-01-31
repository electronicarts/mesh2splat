#include "GaussiansPrepass.hpp"

void GaussiansPrepass::execute(RenderContext& renderContext)
{

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_PREPASS, -1, "GAUSSIAN_SPLATTING_PREPASS");
#endif 
    glUseProgram(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram);

    glUtils::setUniform1f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_stdDev", renderContext.gaussianStd / (float(renderContext.resolutionTarget)));
    glUtils::setUniform3f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_hfov_focal", renderContext.hfov_focal);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,   "u_worldToView", renderContext.viewMat);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,   "u_viewToClip", renderContext.projMat);
    glUtils::setUniform2f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform3f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_camPos", renderContext.camPos);
    glUtils::setUniform1i(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_renderMode", renderContext.renderMode);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.gaussianBufferPostFiltering);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.perQuadTransformationsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, renderContext.drawIndirectBuffer);
    
    //Technically should happen on all of them, right unless some Temporal solution is used?
    GLint size = 0;
    glBindBuffer          (GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &size);

    unsigned int threadGroup_xy = int(size / (sizeof(glm::vec4) * 6));
    glDispatchCompute(threadGroup_xy, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}
