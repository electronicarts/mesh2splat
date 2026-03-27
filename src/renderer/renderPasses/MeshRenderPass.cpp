///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "MeshRenderPass.hpp"

void MeshRenderPass::execute(RenderContext& renderContext)
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 11, -1, "MESH_RENDER_PASS");
#endif 

    GLuint shader = renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::MeshRenderProgram);
    glUseProgram(shader);

    glBindFramebuffer(GL_FRAMEBUFFER, renderContext.meshGBufferFBO);
    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUtils::setUniformMat4(shader, "u_modelToWorld", renderContext.modelMat);
    glUtils::setUniformMat4(shader, "u_worldToView", renderContext.viewMat);
    glUtils::setUniformMat4(shader, "u_viewToClip", renderContext.projMat);
    glUtils::setUniform2f(shader, "u_nearFar", glm::vec2(renderContext.nearPlane, renderContext.farPlane));
    glUtils::setUniform1i(shader, "u_renderMode", renderContext.renderMode);

    for (auto& mesh : renderContext.dataMeshAndGlMesh)
    {
        glUtils::setUniform1i(shader, "hasAlbedoMap", 0);
        glUtils::setUniform1i(shader, "hasNormalMap", 0);
        glUtils::setUniform1i(shader, "hasMetallicRoughnessMap", 0);
        glUtils::setUniform4f(shader, "u_baseColorFactor", mesh.first.material.baseColorFactor);

        if (renderContext.meshToTextureData.find(mesh.first.name) != renderContext.meshToTextureData.end())
        {
            auto& textureMap = renderContext.meshToTextureData.at(mesh.first.name);

            if (textureMap.find(BASE_COLOR_TEXTURE) != textureMap.end())
            {
                glUtils::setTexture2D(shader, "albedoTexture", textureMap.at(BASE_COLOR_TEXTURE).glTextureID, 0);
                glUtils::setUniform1i(shader, "hasAlbedoMap", 1);
            }
            if (textureMap.find(NORMAL_TEXTURE) != textureMap.end())
            {
                glUtils::setTexture2D(shader, "normalTexture", textureMap.at(NORMAL_TEXTURE).glTextureID, 1);
                glUtils::setUniform1i(shader, "hasNormalMap", 1);
            }
            if (textureMap.find(METALLIC_ROUGHNESS_TEXTURE) != textureMap.end())
            {
                glUtils::setTexture2D(shader, "metallicRoughnessTexture", textureMap.at(METALLIC_ROUGHNESS_TEXTURE).glTextureID, 2);
                glUtils::setUniform1i(shader, "hasMetallicRoughnessMap", 1);
            }
        }

        glBindVertexArray(mesh.second.vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh.second.vertexCount);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}
