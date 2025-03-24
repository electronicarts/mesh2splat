#include "DepthPrepass.hpp"

void DepthPrepass::execute(RenderContext& renderContext)
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::DEPTH_PREPASS, -1, "DEPTH_PREPASS");
#endif 

    glUseProgram(renderContext.shaderPrograms.depthPrepassShaderProgram);

    glBindFramebuffer(GL_FRAMEBUFFER, renderContext.depthFBO);
    
    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUtils::setUniformMat4(renderContext.shaderPrograms.depthPrepassShaderProgram,   "u_modelToWorld", renderContext.modelMat);
    glUtils::setUniformMat4(renderContext.shaderPrograms.depthPrepassShaderProgram,   "u_worldToView", renderContext.viewMat);
    glUtils::setUniformMat4(renderContext.shaderPrograms.depthPrepassShaderProgram,   "u_viewToClip", renderContext.projMat);


    for (auto& mesh : renderContext.dataMeshAndGlMesh)
    {
        if (mesh.first.material.baseColorFactor.a == 1.0f) //Exclude non-opaque objects from depth buffer creation
        {
            glBindVertexArray(mesh.second.vao);
            glDrawArrays(GL_TRIANGLES, 0, mesh.second.vertexCount); 
        }
    }

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 

}