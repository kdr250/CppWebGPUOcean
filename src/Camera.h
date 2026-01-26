#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

struct RenderUniforms;

class Camera
{
public:
    bool isDragging;
    float prevX;
    float prevY;
    float currentXTheta;
    float currentYTheta;
    float maxYTheta;
    float minYTheta;
    float sensitivity;
    float currentDistance;
    float maxDistance;
    float minDistance;
    glm::vec3 target;
    float fov;
    float zoomRate;

public:
    Camera();

    void Reset(RenderUniforms& renderUniforms,
               float initDistance,
               glm::vec3 target,
               float fov,
               float zoomRate);

    void RecalculateView(RenderUniforms& renderUniforms);

private:
    glm::mat4 Perspective(float fovY, float aspect, float zNear, float zFar);
    glm::mat4 LookAt(glm::vec3 eye, glm::vec3 target, glm::vec3 up);
    glm::mat4 Inverse(glm::mat4 m);
};
