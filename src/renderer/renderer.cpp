#include "renderer.hpp"

//TODO: create a separete camera class, avoid it bloating and getting too messy

Renderer::Renderer(GLFWwindow* window)
{
    sceneManager = std::make_unique<SceneManager>(renderContext);

    rendererGlfwWindow = window;
    renderPassesOrder = {};
    renderContext = {};
    glGenVertexArrays(1, &(renderContext.vao));
    renderContext.gaussianBuffer            = -1;
    renderContext.drawIndirectBuffer        = -1;
    renderContext.keysBuffer                = -1;
	renderContext.valuesBuffer              = -1;
    renderContext.gaussianBufferSorted      = -1;
    renderContext.normalizedUvSpaceWidth    = 0;
    renderContext.normalizedUvSpaceHeight   = 0;
    renderContext.rendererGlfwWindow        = window; //TODO: this double reference is ugly, refactor

    lastShaderCheckTime      = glfwGetTime();
    //TODO: should this maybe live in the Renderer rather than shader utils? Probably yes
    glUtils::initializeShaderFileMonitoring(
        shaderFiles,
        converterShadersInfo, computeShadersInfo,
        radixSortPrePassShadersInfo, radixSortGatherPassShadersInfo,
        rendering3dgsShadersInfo
    );
    //TODO: now that some more passes are being added I see how this won´t scale at all, need a better way to deal with shader registration and passes
    updateShadersIfNeeded(true);
    
    glGenBuffers(1, &(renderContext.keysBuffer));
    glGenBuffers(1, &(renderContext.valuesBuffer));
    glGenBuffers(1, &(renderContext.gaussianBufferSorted));

    GLsizeiptr maxKeysBytes   = MAX_GAUSSIANS_TO_SORT * sizeof(unsigned int);
    GLsizeiptr maxValuesBytes = MAX_GAUSSIANS_TO_SORT * sizeof(unsigned int);
    GLsizeiptr maxGaussBytes  = MAX_GAUSSIANS_TO_SORT * sizeof(GaussianDataSSBO);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.keysBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxKeysBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.keysBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.valuesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxValuesBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.valuesBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBufferSorted);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxGaussBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, renderContext.gaussianBufferSorted);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    for (size_t i = 0; i < 10; ++i) {
        GLuint query;
        glGenQueries(1, &query);
        renderContext.queryPool.push_back(query);
    }

}

Renderer::~Renderer()
{
    glDeleteVertexArrays(1, &(renderContext.vao));

    glDeleteProgram(renderContext.shaderPrograms.renderShaderProgram);
    glDeleteProgram(renderContext.shaderPrograms.converterShaderProgram);
    glDeleteProgram(renderContext.shaderPrograms.computeShaderProgram);
    glDeleteProgram(renderContext.shaderPrograms.radixSortPrepassProgram);
    glDeleteProgram(renderContext.shaderPrograms.radixSortGatherProgram);

    glDeleteBuffers(1, &(renderContext.gaussianBuffer));
    glDeleteBuffers(1, &(renderContext.drawIndirectBuffer));
    glDeleteBuffers(1, &(renderContext.keysBuffer));
    glDeleteBuffers(1, &(renderContext.valuesBuffer));
    glDeleteBuffers(1, &(renderContext.gaussianBufferSorted));

    for (auto& query : renderContext.queryPool) {
        glDeleteQueries(1, &query);
    }
}

void Renderer::initialize() {
    std::string conversionPassName = "conversion";
    std::string radixSortPassName = "radixSort";
    std::string gaussianSplattingPassName = "gaussianSplatting";

    renderPasses[conversionPassName]          = std::make_unique<ConversionPass>();
    renderPasses[radixSortPassName]           = std::make_unique<RadixSortPass>();
    renderPasses[gaussianSplattingPassName]   = std::make_unique<GaussianSplattingPass>();

    renderPassesOrder = {
        conversionPassName,
        radixSortPassName,
        gaussianSplattingPassName
    };
}

void Renderer::renderFrame()
{
    if (!renderContext.queryPool.empty()) {
        GLuint currentQuery = renderContext.queryPool.front();
        glBeginQuery(GL_TIME_ELAPSED, currentQuery);
    }

    for (auto& renderPassName : renderPassesOrder)
    {
        auto& passPtr = renderPasses[renderPassName];
        if (passPtr->isEnabled())
        {
            passPtr->execute(renderContext);
            passPtr->setIsEnabled(false); //Default to false for next render pass
        }
    }

    if (!renderContext.queryPool.empty()) {
        GLuint currentQuery = renderContext.queryPool.front();
        glEndQuery(GL_TIME_ELAPSED);

        renderContext.queryPool.pop_front();
        renderContext.queryPool.push_back(currentQuery);
    }

    if (renderContext.queryPool.size() > 1) {
        GLuint completedQuery = renderContext.queryPool.front();
        GLuint64 elapsedTime = 0;
        glGetQueryObjectui64v(completedQuery, GL_QUERY_RESULT, &elapsedTime);
        this->gpuFrameTimeMs = static_cast<double>(elapsedTime) / 1e6; // ns to ms
    }
};        

void Renderer::updateTransformations()
{
    int width, height;
    glfwGetFramebufferSize(rendererGlfwWindow, &width, &height);

    float fov = 90.0f;
    float closePlane = 0.01f;
    float farPlane = 100.0f;
    renderContext.projMat = glm::perspective(glm::radians(fov),
                                            (float)width / (float)height,
                                            closePlane, farPlane);
    //TODO: dont think i need this
    glViewport(0, 0, width, height);

    //TODO: refactor, should not be here. Create a separate transformations/camera class
    glm::vec3 cameraPos = computeCameraPosition();
    glm::vec3 target(0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    renderContext.viewMat = glm::lookAt(cameraPos, target, up);
    glm::mat4 model = glm::mat4(1.0);
    renderContext.MVP = renderContext.projMat * renderContext.viewMat * model;

    float htany = tan(glm::radians(fov) / 2);
    float htanx = htany / height * width;
    float focal_z = height / (2 * htany);
    renderContext.hfov_focal = glm::vec3(htanx, htany, focal_z);
}


glm::vec3 Renderer::computeCameraPosition() {
    // Convert angles to radians
    float yawRadians   = glm::radians(yaw);
    float pitchRadians = glm::radians(pitch);

    // Calculate the new camera position in Cartesian coordinates
    float x = distance * cos(pitchRadians) * cos(yawRadians);
    float y = distance * sin(pitchRadians);
    float z = distance * cos(pitchRadians) * sin(yawRadians);
    
    return glm::vec3(x, y, z);
}

void Renderer::clearingPrePass(glm::vec4 clearColor)
{
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 0); //alpha==0 Important for correct blending --> but still front to back expects first DST to be (0,0,0,0)
    //TODO: find way to circumvent first write, as bkg color should not be accounted for in blending --> not sure actually possible using "glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE)"
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::setLastShaderCheckTime(double lastShaderCheckedTime)
{
    this->lastShaderCheckTime = lastShaderCheckedTime;
}

double Renderer::getLastShaderCheckTime()
{
    return lastShaderCheckTime;
}

RenderContext* Renderer::getRenderContext()
{
    return &renderContext;
}

void Renderer::enableRenderPass(std::string renderPassName)
{
    if (auto renderPass = renderPasses.find(renderPassName); renderPass != renderPasses.end())
    {
        renderPass->second->setIsEnabled(true);
    } else {
        std::cerr << "RenderPass: [ "<< renderPassName << " ] not found." << std::endl;
    }
}

void Renderer::setViewportResolutionForConversion(int resolutionTarget)
{
    renderContext.resolutionTarget = resolutionTarget;
}

void Renderer::resetRendererViewportResolution()
{
    int width, height;
    glfwGetFramebufferSize(rendererGlfwWindow, &width, &height);
    renderContext.rendererResolution = glm::ivec2(width, height);
}

void Renderer::setStdDevFromImGui(float stdDev)
{
    renderContext.gaussianStd = stdDev;
}

SceneManager& Renderer::getSceneManager()
{
    return *sceneManager;
}

double Renderer::getTotalGpuFrameTimeMs() const { return gpuFrameTimeMs; }


bool Renderer::updateShadersIfNeeded(bool forceReload) {
    for (auto& entry : shaderFiles) {
        glUtils::ShaderFileInfo& info = entry.second;
        if (forceReload || shaderFileChanged(info) ) {
            // Update timestamp
            info.lastWriteTime                  = glUtils::fs::last_write_time(info.filePath);

            this->renderContext.shaderPrograms.converterShaderProgram   = glUtils::reloadShaderPrograms(converterShadersInfo, this->renderContext.shaderPrograms.converterShaderProgram);
            this->renderContext.shaderPrograms.computeShaderProgram     = glUtils::reloadShaderPrograms(computeShadersInfo, this->renderContext.shaderPrograms.computeShaderProgram);

            this->renderContext.shaderPrograms.radixSortPrepassProgram  = glUtils::reloadShaderPrograms(radixSortPrePassShadersInfo, this->renderContext.shaderPrograms.radixSortPrepassProgram);
            this->renderContext.shaderPrograms.radixSortGatherProgram   = glUtils::reloadShaderPrograms(radixSortGatherPassShadersInfo, this->renderContext.shaderPrograms.radixSortGatherProgram);

            this->renderContext.shaderPrograms.renderShaderProgram      = glUtils::reloadShaderPrograms(rendering3dgsShadersInfo, this->renderContext.shaderPrograms.renderShaderProgram);

            return true; //TODO: ideally it should just reload the programs for which that shader is included, may need dependency for that? Cannot just recompile one program as some are dependant on others
            //TODO P1: investigate this, I am not sure I dont think I need to recreate all programs, I am now convinced I can just do reloadShaderPrograms(info, --> ) need to know how it is saved within the map
            //TODO --> adopt convention in naming of entries in map such that in the "shaderFiles" they are named as the shaderInfo for consistency
        }
    }
    return false;
}

unsigned int Renderer::getGaussianCountFromIndirectBuffer()
{
    if (this->renderContext.drawIndirectBuffer != static_cast<GLuint>(-1))
    {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, this->renderContext.drawIndirectBuffer);
        //TODO: should not need to re-define this buffer each time lol
        struct DrawArraysIndirectCommand {
            GLuint count;        // Number of vertices to draw.
            GLuint instanceCount;    // Number of instances.
            GLuint first;        // Starting index in the vertex buffer.
            GLuint baseInstance; // Base instance for instanced rendering.
        };

        DrawArraysIndirectCommand* cmd = (DrawArraysIndirectCommand*)glMapBufferRange(
            GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawArraysIndirectCommand), GL_MAP_READ_BIT
        );

        unsigned int validCount = cmd->instanceCount;
        glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
        return validCount;
    }
    return 0;

}