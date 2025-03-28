///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "utils.hpp"

#define FAST_SPEED_MOVEMENT 3.5f
#define SLOW_SPEED_MOVEMENT .25f
#define DEFAULT_SPEED_MOVEMENT 1.0f

class Camera {
public:
    Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch);

    glm::mat4 GetViewMatrix() const;
    void ProcessKeyboard(float deltaTime, bool forward, bool backward, bool left, bool right, bool upMove, bool downMove, bool rotateLeftFrontVect, bool rotateRightFrontVect, bool boostSpeed, bool slowSpeed);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);
    float GetFOV() const { return FOV; }
    glm::vec3 GetPosition() const { return Position; }

private:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;
    float Roll;

    float MovementSpeed;
    float MouseSensitivity;
    float FOV;

    void updateCameraVectors();
};
