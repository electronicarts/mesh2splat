#include "GaussianSplattingPass.hpp"

void GaussianSplattingPass::execute(RenderContext& renderContext)
{
    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

	glUseProgram(renderContext.shaderPrograms.renderShaderProgram);

    //TODO: this will work once sorting is working
	glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    //The correct one: from slide deck of Bernard K.
	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);

    //Probably better with indexing, may save some performance
    std::vector<float> quadVertices = {
        // Tr1
        -1.0f, -1.0f,   0.0f,
         -1.0f, 1.0f,   0.0f,
         1.0f,  1.0f,   0.0f,
        // Tr2 
         -1.0f,  -1.0f, 0.0f,
        1.0f,   1.0f,   0.0f,
        1.0f,   -1.0f,  0.0f,
    };

    glBindVertexArray(renderContext.vao);

    GLuint quadVBO;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(float), quadVertices.data(), GL_DYNAMIC_DRAW);

    setUniformMat4(renderContext.shaderPrograms.renderShaderProgram, "u_worldToView", renderContext.viewMat);
    setUniformMat4(renderContext.shaderPrograms.renderShaderProgram, "u_viewToClip", renderContext.projMat);
    setUniform3f(renderContext.shaderPrograms.renderShaderProgram,   "u_hfov_focal", renderContext.hfov_focal);
    setUniform1f(renderContext.shaderPrograms.renderShaderProgram,   "u_std_dev", renderContext.gaussianStd / (float(renderContext.resolutionTarget)));

    //per instance quad data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, renderContext.gaussianBufferSorted);

    //We need to redo this vertex attrib binding as the buffer could have been deleted if the compute/conversion pass was run, and we need to free the data to avoid
    // memory leak. Should structure renderer architecture
    unsigned int stride = sizeof(glm::vec4) * 6; //TODO: ISSUE6 (check for it)
    
    for (int i = 1; i <= 6; ++i) {
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(glm::vec4) * (i - 1)));
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderContext.drawIndirectBuffer);
    
    glDrawArraysIndirect(GL_TRIANGLES, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}