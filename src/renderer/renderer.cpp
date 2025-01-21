#include "renderer.hpp"

//TODO: create a separete camera class, avoid it bloating and getting too messy

Renderer::Renderer()
{
    glGenVertexArrays(1, &pointsVAO);
    converterShaderProgram   = createConverterShaderProgram();
    renderShaderProgram      = createRendererShaderProgram(); 
    computeShaderProgram     = createComputeShaderProgram(); 
    gaussianBuffer           = -1;
    drawIndirectBuffer       = -1;
    normalizedUvSpaceWidth   = 0;
    normalizedUvSpaceHeight  = 0;
}



Renderer::~Renderer()
{
    glDeleteVertexArrays(1, &pointsVAO);
    glDeleteBuffers(1, &gaussianBuffer);
    glDeleteBuffers(1, &drawIndirectBuffer);
    glDeleteProgram(renderShaderProgram);
    glDeleteProgram(converterShaderProgram);
    glDeleteProgram(computeShaderProgram);
}

glm::vec3 Renderer::computeCameraPosition(float yaw, float pitch, float distance) {
    // Convert angles to radians
    float yawRadians   = glm::radians(yaw);
    float pitchRadians = glm::radians(pitch);

    // Calculate the new camera position in Cartesian coordinates
    float x = distance * cos(pitchRadians) * cos(yawRadians);
    float y = distance * sin(pitchRadians);
    float z = distance * cos(pitchRadians) * sin(yawRadians);
    
    return glm::vec3(x, y, z);
}

unsigned int Renderer::getSplatBufferCount(GLuint counterBuffer)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterBuffer);
    unsigned int splatCount = 1000;
    unsigned int* counterPtr = (unsigned int*) glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(unsigned int), GL_MAP_READ_BIT);
    if (counterPtr) {
        splatCount = *counterPtr;
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    return splatCount;
}

void Renderer::run3dgsRenderingPass(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss)
{
    if (gaussianBuffer == static_cast<GLuint>(-1) ||
        drawIndirectBuffer == static_cast<GLuint>(-1) ||
        pointsVAO == 0 ||
        renderShaderProgram == 0) return;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    float fov = 45.0f;
    float closePlane = 0.1f;
    float farPlane = 500.0f;
    glm::mat4 projection = glm::perspective(glm::radians(fov),
                                            (float)width / (float)height,
                                            closePlane, farPlane);

    glViewport(0, 0, width, height);

    //TODO: refactor, should not be here. Create a separate transformations class
    glm::vec3 cameraPos = computeCameraPosition(yaw, pitch, distance);
    glm::vec3 target(0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(cameraPos, target, up);
    glm::mat4 model = glm::mat4(1.0);
    glm::mat4 MVP = projection * view * model;

    //std::vector<float> quadVertices = {
    //    // Tr1
    //    -0.5f, -0.5f, 0.0f,
    //     0.5f, -0.5f, 0.0f,
    //     0.5f,  0.5f, 0.0f,
    //    // Tr2 
    //     0.5f,  0.5f, 0.0f,
    //    -0.5f,  0.5f, 0.0f,
    //    -0.5f, -0.5f, 0.0f,
    //};

    std::vector<float> quadVertices = {
    -0.5f, -0.5f, 0.0f,  // Bottom-left
     0.5f, -0.5f, 0.0f,  // Bottom-right
     0.5f,  0.5f, 0.0f,  // Top-right
    -0.5f,  0.5f, 0.0f,  // Top-left
    };

    glBindVertexArray(pointsVAO);

    GLuint quadVBO, quadEBO;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices.data(), GL_STATIC_DRAW);

    glUseProgram(renderShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(renderShaderProgram, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, gaussianBuffer);
    //We need to redo this vertex attrib binding as the buffer could have been deleted if the compute/conversion pass was run, adn we need to free the data to avoid
    // memory leak. Could use a flag to check if the buffer was freed or not
    unsigned int stride = sizeof(glm::vec4) * 5;
    
    for (int i = 1; i <= 5; ++i) {
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(glm::vec4) * (i - 1)));
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
    glDrawArraysIndirect(GL_TRIANGLE_STRIP, 0);
    //glDrawArraysIndirect(GL_TRIANGLES, 0);

    glBindVertexArray(0);
}
	
void Renderer::clearingPrePass()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderLoop(GLFWwindow* window, ImGuiUI& gui)
{
    //Yeah not greates setup, the logic itself should not probably reside in the gui, but good enough like this.
    //Should implement the concept of a render pass rather than having a specialized class handle one type of pass
    //TODO: make render passes and logic handling more modular
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        clearingPrePass();

        gui.preframe();
        gui.renderUI();

        {
            if (gui.shouldLoadNewMesh()) {
                mesh2SplatConversionHandler.prepareMeshAndUploadToGPU(gui.getFilePath(), gui.getFilePathParentFolder(), dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight);
                for (auto& mesh : dataMeshAndGlMesh)
                {
                    Mesh meshData = mesh.first;
                    GLMesh meshGl = mesh.second;
                    printf("Loading textures into utility std::map\n");
                    loadAllTextureMapImagesIntoMap(meshData.material, textureTypeMap);
                    generateTextures(meshData.material, textureTypeMap);
                }
                std::string meshFilePath(gui.getFilePath());
                if (!meshFilePath.empty()) {
                    mesh2SplatConversionHandler.runConversionPass(
                        meshFilePath, gui.getFilePathParentFolder(),
                        gui.getResolutionTarget(), gui.getGaussianStd(),
                        converterShaderProgram, computeShaderProgram,
                        dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight,
                        textureTypeMap, gaussianBuffer, drawIndirectBuffer
                    );
                }

                gui.setLoadNewMesh(false);
            }

            if (gui.shouldRunConversion()) {
                std::string meshFilePath(gui.getFilePath());
                if (!meshFilePath.empty()) {
                    //Entry point for conversion code
                    mesh2SplatConversionHandler.runConversionPass(
                        meshFilePath, gui.getFilePathParentFolder(),
                        gui.getResolutionTarget(), gui.getGaussianStd(),
                        converterShaderProgram, computeShaderProgram,
                        dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight,
                        textureTypeMap, gaussianBuffer, drawIndirectBuffer
                    );
                }
                gui.setRunConversion(false);
            }

            if (gui.shouldSavePly() && gui.getFilePathParentFolder().size() > 0)
            {
                std::vector<Gaussian3D> gaussian_3d_list;
                savePlyVector(std::string(gui.getFilePathParentFolder()), gaussian_3d_list, gui.getFormatOption());
            }

            //TODO: scen graph related --> should have per render pass a vector of optional parameters and a simple blackboard
            run3dgsRenderingPass(window, pointsVAO, gaussianBuffer, drawIndirectBuffer, renderShaderProgram, gui.getGaussianStd());
        }

        gui.postframe();

        glfwSwapBuffers(window);
    }

    glfwTerminate();
}