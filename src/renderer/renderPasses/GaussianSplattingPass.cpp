///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "GaussianSplattingPass.hpp"

GaussianSplattingPass::GaussianSplattingPass(RenderContext& renderContext)
{
    std::vector<float> quadVertices = {
        -1.0f, -1.0f, 0.0f,// V0
        -1.0f,  1.0f, 0.0f,// V1
         1.0f,  1.0f, 0.0f,// V2
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
    glBindFramebuffer(GL_FRAMEBUFFER, renderContext.gBufferFBO);

    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_RENDER, -1, "GAUSSIAN_SPLATTING_RENDER");
#endif 
	
    GLuint renderShaderProgramID = renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::Rendering3dgsProgram);

    glUseProgram(renderShaderProgramID);

    glUtils::setUniform2f(renderShaderProgramID,     "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform1i(renderShaderProgramID,     "u_renderMode", renderContext.renderMode);


	glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //From deck of Bernard K.: https://3dgstutorial.github.io/3dv_part2.pdf slide 25
    if(renderContext.renderMode == 4) //overdraw
	    glBlendFunc(GL_ONE, GL_ONE);
    else
	    glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);

    glBindVertexArray(renderContext.vao);

    //per-instance (per quad) data
    glBindBuffer(GL_ARRAY_BUFFER, renderContext.perQuadTransformationBufferSorted);

    //We need to redo this vertex attrib binding as the buffer could have been deleted if the compute/conversion pass was run
    unsigned int vec4sPerInstance = 6;
    unsigned int stride = sizeof(glm::vec4) * vec4sPerInstance; //This is the ndc stride
    
    //i=0 is for the per-vertex quad pos, see line 27. Technically we´ll have a byte "hole" between per vertex-data (vec3) and the per-instance one (vec4) considering "(void*)(sizeof(glm::vec4) * (i - 1))" pointer
    for (int i = 1; i <= vec4sPerInstance; ++i) {
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_BLEND);
}