#include "Camera.hpp"

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

