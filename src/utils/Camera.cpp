///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "Camera.hpp"
#include <algorithm>
#include <cmath>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
      MovementSpeed(2.0f),
      MouseSensitivity(0.1f),
      FOV(45.0f) {

    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    Roll = 0;
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

glm::mat4 Camera::GetProjectionMatrix(int width, int height, float nearP, float farP) const {
    if (isOrthographic) {
        float aspect = (float)width / (float)height;
        float halfH = orthoSize;
        float halfW = halfH * aspect;
        return glm::ortho(-halfW, halfW, -halfH, halfH, nearP, farP);
    }
    return glm::perspective(glm::radians(FOV),
                            (float)width / (float)height,
                            nearP, farP);
}

void Camera::ProcessKeyboard(float deltaTime, bool forward, bool backward, bool left, bool right, bool upMove, bool downMove, bool rotateLeftFrontVect, bool rotateRightFrontVect, bool boostSpeed, bool slowSpeed) {
    float speedMultiplier = (boostSpeed ? FAST_SPEED_MOVEMENT : DEFAULT_SPEED_MOVEMENT);
    speedMultiplier = (slowSpeed ? SLOW_SPEED_MOVEMENT : speedMultiplier);

    float velocity = MovementSpeed * deltaTime * speedMultiplier;
    if (forward)
        Position += Front * velocity;
    if (backward)
        Position -= Front * velocity;
    if (left)
        Position -= Right * velocity;
    if (right)
        Position += Right * velocity;
    if (upMove)
        Position += WorldUp * velocity;
    if (downMove)
        Position -= WorldUp * velocity;
    if (rotateRightFrontVect)
        Roll -= 0.5;
    if (rotateLeftFrontVect)
        Roll += 0.5;

    updateCameraVectors();
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    if (constrainPitch) {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset) {
    FOV -= yoffset;
    if (FOV < 1.0f)
        FOV = 1.0f;
    if (FOV > 90.0f)
        FOV = 90.0f;
}

void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    glm::vec3 baseUp = glm::vec3(0.0f, 1.0f, 0.0f);

    //TODO: ideally R and T should rotate the model, rather than rool the camera
    glm::mat4 rollMat = glm::rotate(glm::mat4(1.0f), glm::radians(Roll), Front);
    glm::vec3 rolledUp = glm::vec3(rollMat * glm::vec4(baseUp, 0.0f));

    Right = glm::normalize(glm::cross(Front, rolledUp));

    Up    = glm::normalize(glm::cross(Right, Front));
}

void Camera::FitToBounds(glm::vec3 bboxMin, glm::vec3 bboxMax)
{
    glm::vec3 center = (bboxMin + bboxMax) * 0.5f;
    glm::vec3 size = bboxMax - bboxMin;
    float maxDim = std::max({size.x, size.y, size.z});

    // Default distance: far enough to see entire bounding sphere with current FOV
    float fovRad = glm::radians(FOV);
    float distance = (maxDim / 2.0f) / std::tan(fovRad / 2.0f) * 1.5f; // 1.5x padding

    // Place camera looking at center from a 45-degree angle (top-right-front)
    glm::vec3 offset = glm::normalize(glm::vec3(1.0f, 0.5f, 1.0f)) * distance;
    Position = center + offset;
    Front = glm::normalize(center - Position);

    // Reset Yaw/Pitch to match the new direction
    Pitch = glm::degrees(std::asin(Front.y));
    Yaw = glm::degrees(std::atan2(Front.z, Front.x));
    Roll = 0.0f;

    updateCameraVectors();

    // Set orthographic size to fit the model
    orthoSize = maxDim * 0.6f;
}

void Camera::OrbitYaw(float angleDelta)
{
    Yaw += angleDelta;
    // Normalize to [-180, 180]
    while (Yaw > 180.0f) Yaw -= 360.0f;
    while (Yaw < -180.0f) Yaw += 360.0f;
    updateCameraVectors();
}

void Camera::OrbitPitch(float angleDelta)
{
    Pitch += angleDelta;
    // Clamp to [-90, 90]
    if (Pitch > 90.0f) Pitch = 90.0f;
    if (Pitch < -90.0f) Pitch = -90.0f;
    updateCameraVectors();
}

void Camera::Zoom(float factor)
{
    if (isOrthographic) {
        orthoSize /= factor;
        orthoSize = std::max(orthoSize, 0.01f);
    } else {
        // Move camera along Front direction
        Position -= Front * (factor - 1.0f) * 5.0f;
    }
}

