#include "Camera.h"
#include <glm/gtc/constants.hpp>
#include <glm/ext.hpp>

#include "Application.h"

Camera::Camera() {}

void Camera::Reset(RenderUniforms& renderUniforms,
                   float initDistance,
                   glm::vec3 target,
                   float fov,
                   float zoomRate)
{
    this->isDragging      = false;
    this->prevX           = 0;
    this->prevY           = 0;
    this->currentXTheta   = glm::pi<float>() / 4.0f;
    this->currentYTheta   = -glm::pi<float>() / 12.0f;
    this->maxYTheta       = 0;
    this->minYTheta       = -0.99 * glm::pi<float>() / 2.0f;
    this->sensitivity     = 0.005;
    this->currentDistance = initDistance;
    this->maxDistance     = 2. * this->currentDistance;
    this->minDistance     = 0.3 * this->currentDistance;
    this->target          = target;
    this->fov             = fov;
    this->zoomRate        = zoomRate;

    auto windowSize = renderUniforms.screenSize;
    float aspect    = windowSize.x / windowSize.y;
    auto projection = glm::perspective(this->fov, aspect, 0.1f, 1000.0f);

    renderUniforms.projectionMatrix    = projection;
    renderUniforms.invProjectionMatrix = glm::inverse(projection);

    RecalculateView(renderUniforms);
}

void Camera::RecalculateView(RenderUniforms& renderUniforms)
{
    auto mat = glm::mat4(1.0f);
    mat *= glm::translate(glm::mat4(1.0f), this->target);
    mat *= glm::rotate(glm::mat4(1.0f), this->currentXTheta, glm::vec3(0.0f, 1.0f, 0.0f));
    mat *= glm::rotate(glm::mat4(1.0f), this->currentYTheta, glm::vec3(1.0f, 0.0f, 0.0f));
    mat *= glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, this->currentDistance));

    glm::vec4 position       = mat * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec3 cameraPosition = glm::vec3(position.x, position.y, position.z);

    auto view = glm::lookAt(cameraPosition, this->target, glm::vec3(0.0f, 1.0f, 0.0f));

    renderUniforms.viewMatrix    = view;
    renderUniforms.invViewMatrix = glm::inverse(view);
}
