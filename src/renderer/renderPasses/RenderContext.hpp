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
    glm::mat4 modelMat = glm::mat4(1);
    glm::mat4 viewMat = glm::mat4(1);
    glm::mat4 projMat = glm::mat4(1);
    glm::mat4 MVP; //TODO: yeah, assumes we will render with one single model mat

    glm::mat4 pointLightModel = glm::mat4(1.0); //For now supports only one light source
    bool lightingEnabled;

    glm::vec3 hfov_focal;
    glm::vec3 camPos;
    float nearPlane;
    float farPlane;
    GLFWwindow* rendererGlfwWindow; //TODO: I also need to store this here for now, as I need to reset the viewport DURING the rendering pass as it may ha

    struct ShaderPrograms
    {
        GLuint converterShaderProgram;
        GLuint computeShaderProgram;
        GLuint computeShaderGaussianPrepassProgram;
        GLuint renderShaderProgram;
        GLuint radixSortPrepassProgram;
        GLuint radixSortGatherProgram;
        GLuint deferredRelightingShaderProgram;
    } shaderPrograms;

    int normalizedUvSpaceWidth;
    int normalizedUvSpaceHeight;
    unsigned int referenceResolution;
    unsigned int resolutionTarget; //TODO: confusing with already the
    unsigned int format; //0: from mesh2splat, 1: classic .ply 3dgs, 2: compressedPBR

    // Resources
    GLuint vao;
    GLuint framebuffer;
    size_t vertexCount;
    GLuint* drawBuffers; //TODO: BEWARE
    GLuint gaussianBuffer;
    GLuint gaussianDepthPostFiltering;
    GLuint keysBuffer;
    GLuint perQuadTransformationsBuffer;
    GLuint valuesBuffer;
    GLuint perQuadTransformationBufferSorted;

    GLuint drawIndirectBuffer;
    GLuint atomicCounterBuffer;
    GLuint atomicCounterBufferConversionPass;

    GLint numberOfGaussians;

    // Data Structures
    std::vector<std::pair<utils::Mesh, utils::GLMesh>> dataMeshAndGlMesh;
    std::map<std::string, utils::TextureDataGl> textureTypeMap;
    utils::MaterialGltf material;
    std::vector<utils::GaussianDataSSBO> readGaussians;
    
    std::deque<GLuint> queryPool;

    unsigned int renderMode; //0: color, 1: depth


    //Gbuffer
    GLuint gBufferFBO = 0;

    GLuint gPosition = 0;
    GLuint gNormal = 0;
    GLuint gAlbedo = 0;
    GLuint gDepth = 0;
    GLuint gMetallicRoughness = 0;

};