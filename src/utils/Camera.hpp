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
    glm::mat4 GetProjectionMatrix(int width, int height, float nearP, float farP) const;
    void ProcessKeyboard(float deltaTime, bool forward, bool backward, bool left, bool right, bool upMove, bool downMove, bool rotateLeftFrontVect, bool rotateRightFrontVect, bool boostSpeed, bool slowSpeed);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);
    float GetFOV() const { return FOV; }
    glm::vec3 GetPosition() const { return Position; }
    glm::vec3 GetFront() const { return Front; }
    float GetYaw() const { return Yaw; }
    float GetPitch() const { return Pitch; }

    // Setters for GUI control
    void SetPosition(glm::vec3 pos) { Position = pos; updateCameraVectors(); }
    void SetYawPitch(float yaw, float pitch) { Yaw = yaw; Pitch = pitch; updateCameraVectors(); }
    void SetOrthographic(bool ortho) { isOrthographic = ortho; }
    bool IsOrthographic() const { return isOrthographic; }
    void SetOrthoSize(float size) { orthoSize = size; }
    float GetOrthoSize() const { return orthoSize; }

    // Auto-fit camera to bounding box
    void FitToBounds(glm::vec3 bboxMin, glm::vec3 bboxMax);

    // GUI camera controls (yaw around Y axis, pitch up/down, zoom)
    void OrbitYaw(float angleDelta);  // degrees
    void OrbitPitch(float angleDelta); // degrees, clamped
    void Zoom(float factor);           // >1 zoom in, <1 zoom out

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

    bool isOrthographic = false;
    float orthoSize = 5.0f; // half-height of orthographic view

    void updateCameraVectors();
};
