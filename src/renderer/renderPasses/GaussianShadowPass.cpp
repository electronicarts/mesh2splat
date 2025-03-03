#include "GaussianShadowPass.hpp"
#include "../../radixSort/RadixSort.hpp"

GaussianShadowPass::GaussianShadowPass(RenderContext& renderContext)
{
    glGenTextures(1, &renderContext.m_shadowCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.m_shadowCubemap);

    const int SHADOW_CUBEMAP_SIZE = 1024; 
    for (int i = 0; i < 6; i++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                     SHADOW_CUBEMAP_SIZE, SHADOW_CUBEMAP_SIZE, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_shadowFBO);

    glGenBuffers(1, &(renderContext.pointLightData.perQuadTransformationsUnified));

    for (int face = 0; face < 6; face++) {
        glGenBuffers(1, &renderContext.pointLightData.atomicCounterBufferPerFace[face]);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.pointLightData.atomicCounterBufferPerFace[face]);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
        uint32_t initialValue = 0;
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &initialValue);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2 + face, renderContext.pointLightData.atomicCounterBufferPerFace[face]);
    }

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

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(float), quadVertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, quadIndices.size() * sizeof(GLuint), quadIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glEnableVertexAttribArray(0);

}

void GaussianShadowPass::execute(RenderContext& renderContext)
{
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, renderContext.nearPlane, renderContext.farPlane);

    glm::vec3 lightPos = glm::vec3(renderContext.pointLightModel[3]);

    std::vector<glm::mat4> shadowTransforms;

    shadowTransforms.push_back(
        glm::lookAt(lightPos, lightPos + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)) 
    );
    shadowTransforms.push_back(
        glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0,-1.0, 0.0)) 
    );
    shadowTransforms.push_back(
        glm::lookAt(lightPos, lightPos + glm::vec3(0.0,  1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)) 
    );
    shadowTransforms.push_back(
        glm::lookAt(lightPos, lightPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0,-1.0)) 
    );
    shadowTransforms.push_back(
        glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0,  1.0), glm::vec3(0.0,-1.0, 0.0)) 
    );
    shadowTransforms.push_back(
        glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0,-1.0, 0.0)) 
    );


    #ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_PREPASS, -1, "GAUSSIAN_SPLATTING_SHADOW_PREPASS");
#endif 
    glUseProgram(renderContext.shaderPrograms.shadowPassShaderProgram);

    float std_dev = (renderContext.gaussianStd / (float(renderContext.resolutionTarget)));

    glUtils::setUniform1f(renderContext.shaderPrograms.shadowPassShaderProgram,     "u_stdDev", std_dev);
    glUtils::setUniformMat4v(renderContext.shaderPrograms.shadowPassShaderProgram,  "u_worldToViews", shadowTransforms, 6);
    glUtils::setUniformMat4(renderContext.shaderPrograms.shadowPassShaderProgram,   "u_viewToClip", shadowProj);
    glUtils::setUniform2f(renderContext.shaderPrograms.shadowPassShaderProgram,     "u_resolution", renderContext.rendererResolution);
    glUtils::setUniform1i(renderContext.shaderPrograms.shadowPassShaderProgram,     "u_renderMode", renderContext.renderMode);
    glUtils::setUniform1ui(renderContext.shaderPrograms.shadowPassShaderProgram,    "u_format", renderContext.format);
    glUtils::setUniformMat4(renderContext.shaderPrograms.shadowPassShaderProgram,   "u_modelToWorld", renderContext.modelMat);
    glUtils::setUniform1i(renderContext.shaderPrograms.shadowPassShaderProgram,     "u_gaussianCount", renderContext.numberOfGaussians);
    glUtils::setUniform3f(renderContext.shaderPrograms.shadowPassShaderProgram,     "u_lightPos", glm::vec3(renderContext.pointLightModel[3]));

    glUtils::setUniform2f(renderContext.shaderPrograms.shadowPassShaderProgram,     "u_nearFar", glm::vec2(renderContext.nearPlane, renderContext.farPlane));
           
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.pointLightData.perQuadTransformationsUnified);
    size_t transformationBufferSize = MAX_GAUSSIANS_TO_SORT * sizeof(glm::vec4) * 6;
    glBufferData(GL_SHADER_STORAGE_BUFFER, transformationBufferSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.pointLightData.perQuadTransformationsUnified);

    for (int face = 0; face < 6; face++) {
        uint32_t zeroVal = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.pointLightData.atomicCounterBufferPerFace[face]);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &zeroVal);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2 + face, renderContext.pointLightData.atomicCounterBufferPerFace[face]);
    }
    
    unsigned int totalInvocations = renderContext.numberOfGaussians;

    unsigned int threadsPerGroup = 256;
    unsigned int totalGroupsNeeded = (totalInvocations + threadsPerGroup - 1) / threadsPerGroup;
    unsigned int groupsX = (unsigned int)ceil(sqrt((float)totalGroupsNeeded));
    unsigned int groupsY = (totalGroupsNeeded + groupsX - 1) / groupsX; 
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
    drawToCubeMapFaces(renderContext);
}

void GaussianShadowPass::drawToCubeMapFaces(RenderContext& renderContext)
{
    const int SHADOW_CUBEMAP_SIZE = 1024;
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::GAUSSIAN_SPLATTING_PREPASS, -1, "GAUSSIAN_SPLATTING_SHADOW_CUBEMAP_PASS");
#endif 

    for(int face=0; face < 6; face++)
    {
        //I will assume fully opaque gaussians for the sake of simplicity, we would need something like deep-shadow maps for that (I guess?)
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);

        glViewport(0, 0, SHADOW_CUBEMAP_SIZE, SHADOW_CUBEMAP_SIZE);
	
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, renderContext.m_shadowCubemap, 0);

        glUseProgram(renderContext.shaderPrograms.shadowPassCubemapRender);

        glUtils::setUniform2i(renderContext.shaderPrograms.shadowPassCubemapRender,
                                  "u_resolution", glm::ivec2(SHADOW_CUBEMAP_SIZE, SHADOW_CUBEMAP_SIZE));

        glUtils::setUniform3f(renderContext.shaderPrograms.shadowPassCubemapRender,     "u_lightPos", glm::vec3(renderContext.pointLightModel[3]));
        glUtils::setUniform1f(renderContext.shaderPrograms.shadowPassCubemapRender,     "u_farPlane", renderContext.farPlane);

	    glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(m_vao);

        //per-instance (per quad) data
        glBindBuffer(GL_ARRAY_BUFFER, renderContext.pointLightData.perQuadTransformationsUnified);

        const size_t instancesPerFace = MAX_GAUSSIANS_TO_SORT / 6;
        const size_t instanceStride = 6 * sizeof(glm::vec4);
        size_t faceOffset = face * instancesPerFace * instanceStride;
        
        // For each per-instance attribute (locations 1 .. 6), set the pointer with the face offset.
        const int vec4sPerInstance = 6;

        for (int i = 1; i <= vec4sPerInstance; ++i) {
            size_t attribOffset = faceOffset + (i - 1) * sizeof(glm::vec4);
            glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)attribOffset);
            glEnableVertexAttribArray(i);
            glVertexAttribDivisor(i, 1);
        }

        uint32_t validCount = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.pointLightData.atomicCounterBufferPerFace[face]);
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &validCount);

        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, validCount);
    
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
     }

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
