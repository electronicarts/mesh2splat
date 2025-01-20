#include "renderer.hpp"

//TODO: make this a renderer class, move here all the opengl logic and create a separete camera class, avoid it bloating and getting too messy
float yaw = -90.0f, pitch = 0.0f;
double lastMouseX = 320.0f, lastMouseY = 240.0f;  
bool firstMouse = true;
float cameraRadius = 5.0f; 
float distance = 5.0f;
bool mouseDraggingForRotation;
bool mouseDraggingForPanning;
float panSpeed = 0.01;
glm::vec3 cameraTarget = glm::vec3(0,0,0);

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                mouseDraggingForRotation = true;
                glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            } else if (action == GLFW_RELEASE) {
                mouseDraggingForRotation = false;
            }
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                mouseDraggingForPanning = true;
                glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            } else if (action == GLFW_RELEASE) {
                mouseDraggingForPanning = false;
            }
        }
    }
}

// Cursor position callback to update yaw and pitch during dragging
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (mouseDraggingForRotation) {
            double dx = xpos - lastMouseX;
            double dy = ypos - lastMouseY;

            // Sensitivity factors for smoother control
            float sensitivity = 0.1f;
            yaw += static_cast<float>(dx) * sensitivity;
            pitch += static_cast<float>(dy) * sensitivity;

            // Clamp pitch to avoid flipping the camera
            if (pitch > 89.0f)  pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            lastMouseX = xpos;
            lastMouseY = ypos;
        }

        if (mouseDraggingForPanning)
        {
            double currentRightMouseX = xpos;
            double currentRightMouseY = ypos;


            double dx = currentRightMouseX - lastMouseX;
            double dy = currentRightMouseY - lastMouseY;

            // Calculate camera axes for panning
            glm::vec3 camPos = computeCameraPosition();
            glm::vec3 front = glm::normalize(cameraTarget - camPos);
            glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 cameraRight = glm::normalize(glm::cross(front, worldUp));
            glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, front));

            cameraTarget += -static_cast<float>(dx) * panSpeed * cameraRight
                + static_cast<float>(dy) * panSpeed * cameraUp;

            lastMouseX = currentRightMouseX;
            lastMouseY = currentRightMouseY;
        }
    }
}

// Scroll callback to adjust camera distance (zoom)
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float zoomSpeed = 1.0f;
    distance -= static_cast<float>(yoffset) * zoomSpeed;
    
    // Clamp distance to avoid extreme zoom
    if(distance < 1.0f)  distance = 1.0f;
    if(distance > 150.0f) distance = 150.0f;
}

glm::vec3 computeCameraPosition() {
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


GLuint compute_shader_dispatch(GLuint computeShaderProgram, GLuint* drawBuffers, GLuint gaussianBuffer, unsigned int resolutionTarget)
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
    glUniform1i(glGetUniformLocation(computeShaderProgram, "texPosition"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, drawBuffers[3]);
    glUniform1i(glGetUniformLocation(computeShaderProgram, "texColor"), 1);



    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gaussianBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawIndirectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, drawIndirectBuffer);

    // Dispatch compute work groups
    GLuint groupsX = (GLuint)ceil(resolutionTarget / 16.0);
    GLuint groupsY = (GLuint)ceil(resolutionTarget / 16.0);

    glDispatchCompute(groupsX, groupsY, 1);

    // Ensure compute shader completion
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish(); 
    return drawIndirectBuffer;
}

unsigned int getSplatBufferCount(GLuint counterBuffer)
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

void render_point_cloud(GLFWwindow* window, GLuint pointsVAO, GLuint gaussianBuffer, GLuint drawIndirectBuffer, GLuint renderShaderProgram, float std_gauss)
{
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

    // --- Existing camera and point rendering code----
    glm::vec3 cameraPos = computeCameraPosition();
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
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float)*4));
    glEnableVertexAttribArray(1);

    glBindVertexArray(pointsVAO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, drawIndirectBuffer);
    glDrawArraysIndirect(GL_POINTS, 0);

    //glDrawArrays(GL_POINTS, 0, getSplatBufferCount(counterBuffer)); //Find way to track number of generated gaussians in ssbo
    glBindVertexArray(0);
}
