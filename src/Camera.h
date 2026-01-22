#pragma once

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
};
