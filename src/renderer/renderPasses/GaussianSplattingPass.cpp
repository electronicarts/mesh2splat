#include "GaussianSplattingPass.hpp"

GaussianSplattingPass::GaussianSplattingPass(RenderContext& renderContext)
{
    std::vector<float> quadVertices = {
        -1.0f, -1.0f, 0.0f, // V0
        -1.0f,  1.0f, 0.0f, // V1
         1.0f,  1.0f, 0.0f, // V2
         1.0f, -1.0f, 0.0f  // V3
    };

    std::vector<GLuint> quadIndices = {
        0, 1, 2,
        0, 2, 3
    };

    glBindVertexArray(renderContext.vao);

    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(float), quadVertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &quadEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, quadIndices.size() * sizeof(GLuint), quadIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glEnableVertexAttribArray(0);

}

void GaussianSplattingPass::execute(RenderContext& renderContext)
{
    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_RENDER, -1, "GAUSSIAN_SPLATTING_RENDER");
#endif 
	
    glUseProgram(renderContext.shaderPrograms.renderShaderProgram);

    glUtils::setUniform2f(renderContext.shaderPrograms.renderShaderProgram,     "u_resolution", renderContext.rendererResolution);

    //TODO: this will work once sorting is working
	glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    //The correct one: from slide deck of Bernard K.
	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);

    glBindVertexArray(renderContext.vao);

    //per-instance (per quad) data
    glBindBuffer(GL_ARRAY_BUFFER, renderContext.perQuadTransformationBufferSorted);

    //We need to redo this vertex attrib binding as the buffer could have been deleted if the compute/conversion pass was run, and we need to free the data to avoid
    // memory leak. Should structure renderer architecture
    unsigned int stride = sizeof(glm::vec4) * 4; //This is the ndc stride
    
    for (int i = 1; i <= 4; ++i) {
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(glm::vec4) * (i - 1)));
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderContext.drawIndirectBuffer);

    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
    
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}