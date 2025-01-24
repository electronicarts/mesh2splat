#include "renderer.hpp"

//TODO: create a separete camera class, avoid it bloating and getting too messy

Renderer::Renderer()
{
    glGenVertexArrays(1, &pointsVAO);
    gaussianBuffer           = -1;
    drawIndirectBuffer       = -1;
    keysBuffer               = -1;
	valuesBuffer             = -1;
    gaussianBufferSorted     = -1;
    normalizedUvSpaceWidth   = 0;
    normalizedUvSpaceHeight  = 0;

    lastShaderCheckTime      = glfwGetTime();
    //TODO: should this maybe live in the Renderer rather than shader utils? Probably yes
    initializeShaderFileMonitoring(
        shaderFiles,
        converterShadersInfo, computeShadersInfo,
        radixSortPrePassShadersInfo, radixSortGatherPassShadersInfo,
        rendering3dgsShadersInfo
    );
    //TODO: now that some more passes are being added I see how this won´t scale at all, need a better way to deal with shader registration and passes
    updateShadersIfNeeded(true);

    glGenBuffers(1, &keysBuffer);
    glGenBuffers(1, &valuesBuffer);
    glGenBuffers(1, &gaussianBufferSorted);

    // 2) Suppose you guess a maximum size: e.g. maxElements = 1e6
    GLsizeiptr maxKeysBytes   = MAX_GAUSSIANS_TO_SORT * sizeof(unsigned int);
    GLsizeiptr maxValuesBytes = MAX_GAUSSIANS_TO_SORT * sizeof(unsigned int);
    GLsizeiptr maxGaussBytes  = MAX_GAUSSIANS_TO_SORT * sizeof(GaussianDataSSBO);

    // 3) Bind & allocate
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, keysBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxKeysBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keysBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, valuesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxValuesBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valuesBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBufferSorted);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxGaussBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gaussianBufferSorted);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
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

void Renderer::initializeOpenGLState() {
    //TODO: Better have a way to set these opengl states at the render pass level in next refactor
    //glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LESS);

    
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendEquation(GL_FUNC_ADD);

    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_FRONT);

    //glEnable(GL_MULTISAMPLE);
}

//TODO: kinda bad that I pass parameters to this function that are global variables (in )
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

void debugPrintGaussians(GLuint gaussianBuffer, unsigned int maxPrintCount = 50)
{
    // 1. Bind the buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);

    // 2. Determine how many bytes it holds
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bufferSize);

    // 3. Map the buffer range for reading
    //    (You could also use GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT, etc. 
    //     but here we’ll keep it simple.)
    const GaussianDataSSBO* mapped = static_cast<const GaussianDataSSBO*>(
        glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bufferSize, GL_MAP_READ_BIT)
    );
    if (!mapped) {
        std::cerr << "Failed to map Gaussian SSBO for reading.\n";
        return;
    }

    // 4. Calculate how many Gaussians are actually in this buffer
    size_t gaussianCount = bufferSize / sizeof(GaussianDataSSBO);

    // 5. Print some or all of them
    size_t printCount = std::min<size_t>(gaussianCount, maxPrintCount);
    std::cout << "GaussianBuffer has " << gaussianCount << " entries. Printing first "
              << printCount << ":\n";
    
    for (size_t i = 0; i < printCount; ++i) {
        const GaussianDataSSBO& g = mapped[i];
        std::cout << "Index " << i << ": "
                  << "pos(" << g.position.x << ", " << g.position.y << ", " << g.position.z << ", " << g.position.w << "), "
                  << "col(" << g.color.x << ", " << g.color.y << ", " << g.color.z << ", " << g.color.w << "), "
                  << "scale(" << g.scale.x << ", " << g.scale.y << ", " << g.scale.z << ", " << g.scale.w << ")\n"
                  << "normal(" << g.normal.x << ", " << g.normal.y << ", " << g.normal.z << ", " << g.normal.w << "), "
                  << "rotation(" << g.rotation.x << ", " << g.rotation.y << ", " << g.rotation.z << ", " << g.rotation.w << "), "
                  << "pbr(" << g.pbr.x << ", " << g.pbr.y << ", " << g.pbr.z << ", " << g.pbr.w << ")\n";
    }

    // 6. Unmap the buffer when done
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

// Debug function to read and print the contents of two SSBOs (keys & values).
//  - keySsbo:    The OpenGL buffer ID for the keys.
//  - valSsbo:    The OpenGL buffer ID for the values.
//  - count:      Number of elements in each buffer (keys/values are both size `count`).
//  - maxPrint:   If you have a huge buffer, you might limit how many entries to print.
void debugPrintKeysValues(GLuint keySsbo, GLuint valSsbo, size_t count, size_t maxPrint = 50)
{
    // Sanity clamp: if count < maxPrint, just print them all
    if (count < maxPrint) {
        maxPrint = count;
    }

    // 1. Read back the key buffer
    std::vector<GLuint> keys(count);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, keySsbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(GLuint), keys.data());

    // 2. Read back the value buffer
    std::vector<GLuint> vals(count);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, valSsbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(GLuint), vals.data());

    // 3. Print a small header
    printf("===== Debug Print of Keys and Values (showing up to %zu of %zu) =====\n", maxPrint, count);
    printf("Index |  Key (uint)  |  Value (uint)\n");
    printf("------|--------------|--------------\n");

    // 4. Print the first `maxPrint` entries
    for (size_t i = 0; i < maxPrint; ++i)
    {
        printf("%5zu | %10u  | %10u\n", i, keys[i], vals[i]);
    }

    // 5. If we limited the output, let the user know
    if (maxPrint < count) {
        printf("... (truncated; total %zu)\n", count);
    }

    printf("\n");
}


//TODO: no need to pass the renderShaderProgram, its a member var
void Renderer::run3dgsRenderingPass(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss, int resolutionTarget)
{
    //TODO: take inspo for StopThePop: https://arxiv.org/pdf/2402.00525
    //When sorting consider that a global sort based on mean depth in view space its consistent during translation
    if (gaussianBuffer == static_cast<GLuint>(-1) ||
        drawIndirectBuffer == static_cast<GLuint>(-1) ||
        pointsVAO == 0 ||
        renderShaderProgram == 0) return;

    //CAMERA SETUP
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    float fov = 90.0f;
    float closePlane = 0.01f;
    float farPlane = 100.0f;
    glm::mat4 projection = glm::perspective(glm::radians(fov),
                                            (float)width / (float)height,
                                            closePlane, farPlane);

    glViewport(0, 0, width, height);

    //TODO: refactor, should not be here. Create a separate transformations/camera class
    glm::vec3 cameraPos = computeCameraPosition(yaw, pitch, distance);
    glm::vec3 target(0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(cameraPos, target, up);
    glm::mat4 model = glm::mat4(0.9f);
    glm::mat4 MVP = projection * view * model;

    float htany = tan(glm::radians(fov) / 2);
    float htanx = htany / height * width;
    float focal_z = height / (2 * htany);
    glm::vec3 hfov_focal = glm::vec3(htanx, htany, focal_z);


    //READ INFO FROM SSBO BUFFER FOR: RADIX SORT PREPASS - RADIX SORT - RADIX SORT POST PASS
    
    glFinish();

    //------------- RADIX SORT PREPASS ----------------
    glBindBuffer(GL_ARRAY_BUFFER, gaussianBuffer);
    
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufferSize);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
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
    
    //std::vector<GaussianDataSSBO> gaussians(validCount);
    //glGetBufferSubData(GL_ARRAY_BUFFER, 0, validCount * sizeof(GaussianDataSSBO), gaussians.data());

    // Transform Gaussian positions to view space and apply global sort
    glUseProgram(radixSortPrepassProgram);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gaussianBuffer);
    //debugPrintGaussians(gaussianBuffer, 10000);
    setUniformMat4(radixSortPrepassProgram, "u_view", view);
    setUniform1ui(radixSortPrepassProgram, "u_count", validCount);
    setupKeysBufferSsbo(validCount, keysBuffer, 1);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, keysBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keysBuffer);
    setupValuesBufferSsbo(validCount, valuesBuffer, 2);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, valuesBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valuesBuffer);
    unsigned int threadGroup_xy = (validCount + 255) / 256;
    glDispatchCompute(threadGroup_xy, 1, 1);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    //debugPrintKeysValues(keysBuffer, valuesBuffer, validCount, 10000);
    //------------- RADIX SORT PASS ----------------
    
    glu::RadixSort sorter;
    sorter(keysBuffer, valuesBuffer, validCount);
    //debugPrintKeysValues(keysBuffer, valuesBuffer, validCount, 10000);
    //------------- RADIX SORT GATHER PASS ---------------- //TODO: I think I could actually not need to gather all the gaussians in a new buffer and simply use indices into the old one during rendering
    glUseProgram(radixSortGatherProgram);
    setUniform1ui(radixSortPrepassProgram, "u_count", validCount);
    
    ; //TODO: should not create it here, should have rather a persistent buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gaussianBuffer);
    setupSortedBufferSsbo(validCount, gaussianBufferSorted, 1); // <-- last int is binding pos
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBufferSorted);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valuesBuffer);
    glDispatchCompute(threadGroup_xy, 1, 1);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    //debugPrintGaussians(gaussianBufferSorted, 10000);
    
    //------------- END RADIX SORT STAGE -------------

    glUseProgram(renderShaderProgram);

    //TODO: this will work once sorting is working
	glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    //The correct one: from slide deck of Bernard K.
	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);


    //Probably better with indexing, may save some performance
    std::vector<float> quadVertices = {
        // Tr1
        -1.0f, -1.0f,   0.0f,
         -1.0f, 1.0f,   0.0f,
         1.0f,  1.0f,   0.0f,
        // Tr2 
         -1.0f,  -1.0f, 0.0f,
        1.0f,   1.0f,   0.0f,
        1.0f,   -1.0f,  0.0f,
    };

    glBindVertexArray(pointsVAO);

    GLuint quadVBO;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(float), quadVertices.data(), GL_DYNAMIC_DRAW);

    //TODO: should name all uniforms with this convention for clarity
    setUniformMat4(renderShaderProgram, "u_MVP", MVP);
    setUniformMat4(renderShaderProgram, "u_worldToView", view);
    setUniformMat4(renderShaderProgram, "u_objectToWorld", model);
    setUniformMat4(renderShaderProgram, "u_viewToClip", projection);
    setUniform2f(renderShaderProgram,   "u_resolution", glm::ivec2(width, height));
    setUniform3f(renderShaderProgram,   "u_hfov_focal", hfov_focal);
    setUniform1f(renderShaderProgram,   "u_std_dev", std_gauss / (float(resolutionTarget)));

    //per instance quad data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, gaussianBufferSorted);

    //We need to redo this vertex attrib binding as the buffer could have been deleted if the compute/conversion pass was run, and we need to free the data to avoid
    // memory leak. Should structure renderer architecture
    unsigned int stride = sizeof(glm::vec4) * 6; //TODO: ISSUE6 (check for it)
    
    for (int i = 1; i <= 6; ++i) {
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(glm::vec4) * (i - 1)));
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
    
    glDrawArraysIndirect(GL_TRIANGLES, 0); //instance parameters set in framBufferReaderCS.glsl
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}
	
void Renderer::clearingPrePass(glm::vec4 clearColor)
{
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 0); //alpha==0 Important for correct blending --> but still front to back expects first DST to be (0,0,0,0)
    //TODO: find way to circumvent first write, as bkg color should not be accounted for in blending --> not sure actually possible using "glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE)"
    glClear(GL_COLOR_BUFFER_BIT);
}

bool Renderer::updateShadersIfNeeded(bool forceReload) {
    for (auto& entry : shaderFiles) {
        ShaderFileInfo& info = entry.second;
        if (forceReload || shaderFileChanged(info) ) {
            // Update timestamp
            info.lastWriteTime                  = fs::last_write_time(info.filePath);
            
            this->converterShaderProgram        = reloadShaderPrograms(converterShadersInfo,             this->converterShaderProgram);
            this->computeShaderProgram          = reloadShaderPrograms(computeShadersInfo,               this->computeShaderProgram);

            this->radixSortPrepassProgram       = reloadShaderPrograms(radixSortPrePassShadersInfo,      this->radixSortPrepassProgram);
            this->radixSortGatherProgram        = reloadShaderPrograms(radixSortGatherPassShadersInfo,   this->radixSortGatherProgram);

            this->renderShaderProgram           = reloadShaderPrograms(rendering3dgsShadersInfo,         this->renderShaderProgram);

            return true; //TODO: ideally it should just reload the programs for which that shader is included, may need dependency for that? Cannot just recompile one program as some are dependant on others
            //TODO P1: investigate this, I am not sure I dont think I need to recreate all programs, I am now convinced I can just do reloadShaderPrograms(info, --> ) need to know how it is saved within the map
            //TODO --> adopt convention in naming of entries in map such that in the "shaderFiles" they are named as the shaderInfo for consistency
        }
    }
    return false;
}

//TODO: implement shader hot-reloading
void Renderer::renderLoop(GLFWwindow* window, ImGuiUI& gui)
{
    //Yeah not greates setup, the logic itself should not probably reside in the gui, but good enough like this.
    //Should implement the concept of a render pass rather than having a specialized class handle one type of pass
    //TODO: make render passes and logic handling more modular, this is very fixed and wobbly

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        clearingPrePass(gui.getSceneBackgroundColor());

        gui.preframe();
        gui.renderUI();

        double currentTime = glfwGetTime();
        if (currentTime - lastShaderCheckTime > 1.0) {
            gui.setRunConversion(updateShadersIfNeeded());
            lastShaderCheckTime = currentTime;
        }

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

            if (gui.shouldSavePly() && !gui.getFilePathParentFolder().empty())
            {
                GaussianDataSSBO* gaussianData = nullptr;
                unsigned int gaussianCount;
                read3dgsDataFromSsboBuffer(drawIndirectBuffer, gaussianBuffer, gaussianData, gaussianCount);
                if (gaussianData) {
                    writeBinaryPlyStandardFormatFromSSBO(gui.getFullFilePathDestination(), gaussianData, gaussianCount);
                }
                gui.setShouldSavePly(false);
            }

            //TODO: should have per render passes class in renderer and a scene object + a vector of optional parameters and a blackboard
            run3dgsRenderingPass(window, pointsVAO, gaussianBuffer, drawIndirectBuffer, renderShaderProgram, gui.getGaussianStd(), gui.getResolutionTarget());
        }

        gui.postframe();

        glfwSwapBuffers(window);
    }

    glfwTerminate();
}