#include "GaussianSplattingPass.hpp"

void GaussianSplattingPass::execute(RenderContext& renderContext)
{
    computePrepass(renderContext);

    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_RENDER, -1, "GAUSSIAN_SPLATTING_RENDER");
#endif 
	
    glUseProgram(renderContext.shaderPrograms.renderShaderProgram);

    //TODO: this will work once sorting is working
	glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    //The correct one: from slide deck of Bernard K.
	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);

    
    std::vector<float> quadVertices = {
        -1.0f, -1.0f, 0.0f, // V0
        -1.0f,  1.0f, 0.0f, // V1
         1.0f,  1.0f, 0.0f, // V2
         1.0f, -1.0f, 0.0f  // V3
    };

    // Define quad indices
    std::vector<GLuint> quadIndices = {
        0, 1, 2,
        0, 2, 3
    };

    glBindVertexArray(renderContext.vao);

    GLuint quadVBO;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(float), quadVertices.data(), GL_STATIC_DRAW);

    // Generate and bind EBO
    GLuint quadEBO;
    glGenBuffers(1, &quadEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, quadIndices.size() * sizeof(GLuint), quadIndices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    //per instance quad data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glEnableVertexAttribArray(0);

    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);

    glBindBuffer(GL_ARRAY_BUFFER, renderContext.perQuadTransformationsBuffer);

    //We need to redo this vertex attrib binding as the buffer could have been deleted if the compute/conversion pass was run, and we need to free the data to avoid
    // memory leak. Should structure renderer architecture
    unsigned int stride = sizeof(glm::vec4) * 3;
    
    for (int i = 1; i <= 3; ++i) {
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(glm::vec4) * (i - 1)));
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderContext.drawIndirectBuffer);
    
    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
    
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void GaussianSplattingPass::computePrepass(RenderContext& renderContext)
{

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_PREPASS, -1, "GAUSSIAN_SPLATTING_PREPASS");
#endif 

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderContext.drawIndirectBuffer);

    DrawElementsIndirectCommand* cmd = (DrawElementsIndirectCommand*)glMapBufferRange(
        GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawElementsIndirectCommand), GL_MAP_READ_BIT
    );


    unsigned int validCount = cmd->instanceCount;
    glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);

    // Transform Gaussian positions to view space and apply global sort
    glUseProgram(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram);

    glUtils::setUniform1f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram, "u_stdDev", renderContext.gaussianStd / (float(renderContext.resolutionTarget)));
    glUtils::setUniform3f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram, "u_hfov_focal", renderContext.hfov_focal);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram, "u_worldToView", renderContext.viewMat);
    glUtils::setUniformMat4(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram, "u_viewToClip", renderContext.projMat);
    glUtils::setUniform2f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram, "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform3f(renderContext.shaderPrograms.computeShaderGaussianPrepassProgram, "u_camPos", renderContext.camPos);



    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBufferSorted);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBufferSorted);

    if(validCount > MAX_GAUSSIANS_TO_SORT) glUtils::resizeAndBindToPosSSBO<glm::vec4>(validCount * 3, renderContext.perQuadTransformationsBuffer, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.perQuadTransformationsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.perQuadTransformationsBuffer);
    
    unsigned int threadGroup_xy = (validCount + 255) / 256;
    glDispatchCompute(threadGroup_xy, 1, 1);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}