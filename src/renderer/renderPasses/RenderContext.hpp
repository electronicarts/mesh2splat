#pragma once

#include "../../utils/utils.hpp"
#include "../../utils/glUtils.hpp"

//TODO: split up in sub contexts
struct RenderContext {
    // Parameters
    std::string meshFilePath;
    std::string baseFolder;
    int resolution;
    glm::ivec2 rendererResolution;
    float gaussianStd;
    glm::mat4 viewMat;
    glm::mat4 projMat;
    glm::mat4 MVP; //TODO: yeah, assumes we will render with one single model mat
    glm::vec3 hfov_focal;
    GLFWwindow* rendererGlfwWindow; //TODO: I also need to store this here for now, as I need to reset the viewport DURING the rendering pass as it may ha

    struct ShaderPrograms
    {
        GLuint converterShaderProgram;
        GLuint computeShaderProgram;
        GLuint renderShaderProgram;
        GLuint radixSortPrepassProgram;
        GLuint radixSortGatherProgram;
    } shaderPrograms;

    int normalizedUvSpaceWidth;
    int normalizedUvSpaceHeight;
    unsigned int referenceResolution;
    unsigned int resolutionTarget; //TODO: confusing with already the

    // Resources
    GLuint vao;
    GLuint framebuffer;
    size_t vertexCount;
    GLuint* drawBuffers; //TODO: BEWARE
    GLuint gaussianBuffer;
    GLuint keysBuffer;
    GLuint valuesBuffer;
    GLuint gaussianBufferSorted;
    GLuint drawIndirectBuffer;

    // Data Structures
    std::vector<std::pair<Mesh, GLMesh>> dataMeshAndGlMesh;
    std::map<std::string, TextureDataGl> textureTypeMap;
    MaterialGltf material;
    
    std::deque<GLuint> queryPool;
};