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

void GaussianRelightingPass::execute(RenderContext& renderContext)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, renderContext.rendererResolution.x, renderContext.rendererResolution.y);

#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_RELIGHTING, -1, "GAUSSIAN_SPLATTING_DEFERRED_PASS");
#endif 

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLuint deferredRelightingShaderProgramID = renderContext.shaderRegistry.getProgramID(glUtils::ShaderProgramTypes::DeferredRelightingPassProgram);


    glUseProgram(deferredRelightingShaderProgramID);

    glUtils::setTexture2D(deferredRelightingShaderProgramID, "gPosition", renderContext.gPosition, 0);
    glUtils::setTexture2D(deferredRelightingShaderProgramID, "gNormal", renderContext.gNormal, 1);
    glUtils::setTexture2D(deferredRelightingShaderProgramID, "gAlbedo", renderContext.gAlbedo, 2);
    glUtils::setTexture2D(deferredRelightingShaderProgramID, "gDepth", renderContext.gDepth, 3);
    glUtils::setTexture2D(deferredRelightingShaderProgramID, "gMetallicRoughness", renderContext.gMetallicRoughness, 4);
    
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.pointLightData.m_shadowCubemap);
    glUniform1i(glGetUniformLocation(deferredRelightingShaderProgramID, "u_shadowCubemap"), 5);


    glUtils::setUniform3f(deferredRelightingShaderProgramID, "u_LightPosition", glm::vec3(renderContext.pointLightData.pointLightModel[3]));
    glUtils::setUniformMat4(deferredRelightingShaderProgramID, "u_clipToView", glm::inverse(renderContext.projMat));
    glUtils::setUniformMat4(deferredRelightingShaderProgramID, "u_worldToView", renderContext.viewMat);
    glUtils::setUniform2i(deferredRelightingShaderProgramID, "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform3f(deferredRelightingShaderProgramID, "u_camPos", renderContext.camPos);
    glUtils::setUniform1i(deferredRelightingShaderProgramID, "u_isLightingEnalbed", renderContext.pointLightData.lightingEnabled);
    glUtils::setUniform1f(deferredRelightingShaderProgramID, "u_farPlane", renderContext.farPlane);
    glUtils::setUniform1f(deferredRelightingShaderProgramID, "u_lightIntensity", renderContext.pointLightData.lightIntensity);
    glUtils::setUniform1i(deferredRelightingShaderProgramID, "u_renderMode", renderContext.renderMode);
    glUtils::setUniform3f(deferredRelightingShaderProgramID, "u_lightColor", renderContext.pointLightData.lightColor);

    glBindVertexArray(m_fullscreenQuadVAO);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 

    glBindVertexArray(0);
}
