///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "GaussianRelightingPass.hpp"

GaussianRelightingPass::GaussianRelightingPass()
{
    float quadVertices[] = {
        //    Pos           UV
        -1.0f,  1.0f,  0.0f, 1.0f,  
        -1.0f, -1.0f,  0.0f, 0.0f,  
         1.0f,  1.0f,  1.0f, 1.0f,  
         1.0f, -1.0f,  1.0f, 0.0f   
    };
    unsigned int quadIndices[] = {
        0, 1, 2, 
        1, 3, 2  
    };

    glGenVertexArrays(1, &m_fullscreenQuadVAO);
    glGenBuffers(1, &m_fullscreenQuadVBO);
    glGenBuffers(1, &m_fullscreenQuadEBO);

    glBindVertexArray(m_fullscreenQuadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_fullscreenQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fullscreenQuadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

}

void GaussianRelightingPass::bindGBufferAndDraw(
    GLuint shader,
    RenderContext& renderContext,
    GLuint positionTex, GLuint normalTex, GLuint albedoTex, GLuint depthTex, GLuint metallicRoughnessTex)
{
    glUtils::setTexture2D(shader, "gPosition", positionTex, 0);
    glUtils::setTexture2D(shader, "gNormal", normalTex, 1);
    glUtils::setTexture2D(shader, "gAlbedo", albedoTex, 2);
    glUtils::setTexture2D(shader, "gDepth", depthTex, 3);
    glUtils::setTexture2D(shader, "gMetallicRoughness", metallicRoughnessTex, 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.pointLightData.m_shadowCubemap);
    glUniform1i(glGetUniformLocation(shader, "u_shadowCubemap"), 5);

    glBindVertexArray(m_fullscreenQuadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void GaussianRelightingPass::setLightingUniforms(GLuint shader, RenderContext& renderContext)
{
    glUtils::setUniform3f(shader, "u_LightPosition", glm::vec3(renderContext.pointLightData.pointLightModel[3]));
    glUtils::setUniformMat4(shader, "u_clipToView", glm::inverse(renderContext.projMat));
    glUtils::setUniformMat4(shader, "u_worldToView", renderContext.viewMat);
    glUtils::setUniform2i(shader, "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform3f(shader, "u_camPos", renderContext.camPos);
    glUtils::setUniform1i(shader, "u_isLightingEnalbed", renderContext.pointLightData.lightingEnabled);
    glUtils::setUniform1f(shader, "u_farPlane", renderContext.farPlane);
    glUtils::setUniform1f(shader, "u_lightIntensity", renderContext.pointLightData.lightIntensity);
    glUtils::setUniform1i(shader, "u_renderMode", renderContext.renderMode);
    glUtils::setUniform3f(shader, "u_lightColor", renderContext.pointLightData.lightColor);
}

void GaussianRelightingPass::execute(RenderContext& renderContext)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_RELIGHTING, -1, "GAUSSIAN_SPLATTING_DEFERRED_PASS");
#endif 

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    GLuint shader = renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::DeferredRelightingPassProgram);
    glUseProgram(shader);
    setLightingUniforms(shader, renderContext);

    if (renderContext.splitScreenEnabled && renderContext.meshGBufferFBO != 0)
    {
        int w = renderContext.rendererResolution.x;
        int h = renderContext.rendererResolution.y;
        int splitPixelX = static_cast<int>(renderContext.splitScreenPosition * w);

        // --- Fill stencil: left half = 1 (mesh), right half = 0 (splat) ---
        glEnable(GL_STENCIL_TEST);
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        // Use scissor to write 1 into left half only
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, splitPixelX, h);
        glClearStencil(1);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        // Stencil is now: 1 on left (mesh), 0 on right (splat)
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        // --- Pass 1: Splat side (stencil == 0, right) ---
        glStencilFunc(GL_EQUAL, 0, 0xFF);
        bindGBufferAndDraw(shader, renderContext,
            renderContext.gPosition, renderContext.gNormal,
            renderContext.gAlbedo, renderContext.gDepth,
            renderContext.gMetallicRoughness);

        // --- Pass 2: Mesh side (stencil == 1, left) ---
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        bindGBufferAndDraw(shader, renderContext,
            renderContext.meshGPosition, renderContext.meshGNormal,
            renderContext.meshGAlbedo, renderContext.meshGDepth,
            renderContext.meshGMetallicRoughness);

        glDisable(GL_STENCIL_TEST);

        // --- Draw divider line via scissored clear ---
        int dividerWidth = 2;
        int dividerX = std::max(0, splitPixelX - dividerWidth / 2);
        glEnable(GL_SCISSOR_TEST);
        glScissor(dividerX, 0, dividerWidth, h);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    }
    else
    {
        // Normal single-pass rendering (no split-screen)
        bindGBufferAndDraw(shader, renderContext,
            renderContext.gPosition, renderContext.gNormal,
            renderContext.gAlbedo, renderContext.gDepth,
            renderContext.gMetallicRoughness);
    }

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 

    glBindVertexArray(0);
}
