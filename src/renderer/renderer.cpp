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
    int normalizedUvSpaceWidth = 0;
    int normalizedUvSpaceHeight = 0;
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
    //return cameraTarget - glm::vec3(x, y, z);
}


GLuint Renderer::compute_shader_dispatch(GLuint computeShaderProgram, GLuint* drawBuffers, GLuint gaussianBuffer, unsigned int resolutionTarget)
{
    struct DrawArraysIndirectCommand {
        GLuint count;        // Number of vertices to draw.
        GLuint primCount;    // Number of instances.
        GLuint first;        // Starting index in the vertex buffer.
        GLuint baseInstance; // Base instance for instanced rendering.
    };

    GLuint drawIndirectBuffer;
    glGenBuffers(1, &drawIndirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawArraysIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

    glUseProgram(computeShaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[0]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "texPositionAndScaleX"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[1]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "scaleZAndNormal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[2]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "rotationAsQuat"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[3]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "texColor"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[4]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "pbrAndScaleY"), 4);


    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gaussianBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawIndirectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, drawIndirectBuffer);

    // Dispatch compute work groups
    GLuint groupsX = (GLuint)ceil(resolutionTarget / 16.0);
    GLuint groupsY = (GLuint)ceil(resolutionTarget / 16.0);

    glDispatchCompute(groupsX, groupsY, 1);

    // Ensure compute shader completion
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish(); 
    return drawIndirectBuffer;
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

void Renderer::render_point_cloud(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss)
{
    if (gaussianBuffer == static_cast<GLuint>(-1) ||
        drawIndirectBuffer == static_cast<GLuint>(-1) ||
        pointsVAO == 0 ||
        renderShaderProgram == 0) return;

    glFinish();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    float fov = 45.0f;
    float closePlane = 0.1f;
    float farPlane = 500.0f;
    glm::mat4 projection = glm::perspective(glm::radians(fov),
                                            (float)width / (float)height,
                                            closePlane, farPlane);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, width, height);

    // --- Existing camera and point rendering code---- TODO: refactor, should not be here
    glm::vec3 cameraPos = computeCameraPosition(yaw, pitch, distance);
    glm::vec3 target(0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(cameraPos, target, up);
    glm::mat4 model = glm::mat4(1.0);
    glm::mat4 MVP = projection * view * model;

    glUseProgram(renderShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(renderShaderProgram, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));

    GLint stdGaussLocation = glGetUniformLocation(renderShaderProgram, "std_gauss");
    if(stdGaussLocation != -1) {
        glUniform1f(stdGaussLocation, std_gauss);
    } else {
        std::cerr << "Warning: std_gauss uniform not found in compute shader." << std::endl;
    }

    glBindVertexArray(pointsVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gaussianBuffer);
    unsigned int stride = sizeof(glm::vec4) * 5;
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*4));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*8));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*12));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*16));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*20));
    glEnableVertexAttribArray(5);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
    glDrawArraysIndirect(GL_POINTS, 0);

    glBindVertexArray(0);
}

void Renderer::renderLoop(GLFWwindow* window, ImGuiUI& gui)
{
    //Yeah not greates setup, the logic itself should not probably reside in the gui, but good enough like this.
    //Should implement the concept of a render pass rather than having a specialized class handle one type of pass
    //TODO: make render passes and logic handling more modular
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        gui.renderUI();
        if (gui.shouldLoadNewMesh()) {
                mesh2SplatConversionHandler.prepareMeshAndUploadToGPU(gui.getFilePath(), gui.getFilePathParentFolder(), dataMeshAndGlMesh, normalizedUvSpaceWidth, normalizedUvSpaceHeight);
                for (auto& mesh : dataMeshAndGlMesh)
                {
                    Mesh meshData = mesh.first;
                    GLMesh meshGl = mesh.second;
                    printf("Loading textures into utility std::map\n");
                    loadAllTextureMapImagesIntoMap(meshData.material, textureTypeMap);
                    generateTextures( meshData.material, textureTypeMap );
                }
                std::string meshFilePath(gui.getFilePath());
                //First conversion so when loading model it already visualizes it
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

        ImGui::Render();
        
        render_point_cloud(window, pointsVAO, gaussianBuffer, drawIndirectBuffer, renderShaderProgram, gui.getGaussianStd());

        // Render ImGui on top
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glfwTerminate();
}