#pragma once

#include "../../utils/utils.hpp"
#include "../../utils/glUtils.hpp"

enum PassesDebugIDs
{
    CONVERSION_PASS                 = 0,
    CONVERSION_AGGREGATION_PASS     = 1,
    RADIX_SORT_KEYSVALUE            = 2,
    RADIX_SORT_MAIN                 = 3,
    RADIX_SORT_GATHER               = 4,
    GAUSSIAN_SPLATTING_PREPASS      = 5,
    GAUSSIAN_SPLATTING_RENDER       = 6
};

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
    glm::vec3 camPos;
    GLFWwindow* rendererGlfwWindow; //TODO: I also need to store this here for now, as I need to reset the viewport DURING the rendering pass as it may ha

    struct ShaderPrograms
    {
        GLuint converterShaderProgram;
        GLuint computeShaderProgram;
        GLuint computeShaderGaussianPrepassProgram;
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
    GLuint gaussianBufferPostFiltering;
    GLuint keysBuffer;
    GLuint perQuadTransformationsBuffer;
    GLuint valuesBuffer;
    GLuint perQuadTransformationBufferSorted;

    GLuint drawIndirectBuffer;
    GLuint atomicCounterBuffer;

    // Data Structures
    std::vector<std::pair<Mesh, GLMesh>> dataMeshAndGlMesh;
    std::map<std::string, TextureDataGl> textureTypeMap;
    MaterialGltf material;
    std::vector<GaussianDataSSBO> readGaussians;
    
    std::deque<GLuint> queryPool;

    unsigned int renderMode; //0: color, 1: depth
};