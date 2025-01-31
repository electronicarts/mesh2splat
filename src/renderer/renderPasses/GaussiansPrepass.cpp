#include "GaussiansPrepass.hpp"

void GaussiansPrepass::execute(RenderContext& renderContext)
{

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_PREPASS, -1, "GAUSSIAN_SPLATTING_PREPASS");
#endif 
    glUseProgram(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram);

    float std_dev = (renderContext.gaussianStd / (float(renderContext.resolutionTarget)));

    glUtils::setUniform1f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_stdDev", std_dev);
    glUtils::setUniform3f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_hfov_focal", renderContext.hfov_focal);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,   "u_worldToView", renderContext.viewMat);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,   "u_viewToClip", renderContext.projMat);
    glUtils::setUniform2f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform3f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_camPos", renderContext.camPos);
    glUtils::setUniform1i(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_renderMode", renderContext.renderMode);
    glUtils::setUniform1ui(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram,     "u_format", renderContext.format);
        

    GLint size = 0;
    glBindBuffer          (GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &size);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.gaussianBufferPostFiltering);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.perQuadTransformationsBuffer);
    
    glUtils::resetAtomicCounter(renderContext.atomicCounterBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, renderContext.atomicCounterBuffer);
    
    unsigned int threadGroup_xy = int(size / (sizeof(glm::vec4) * 6));

    glDispatchCompute(threadGroup_xy, 1, 1);

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}
