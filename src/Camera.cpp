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
    auto projection = Perspective(this->fov, aspect, 0.1f, 1000.0f);

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

    auto view = LookAt(cameraPosition, this->target, glm::vec3(0.0f, 1.0f, 0.0f));

    renderUniforms.viewMatrix    = view;
    renderUniforms.invViewMatrix = glm::inverse(view);
}

glm::mat4 Camera::Perspective(float fovY, float aspect, float zNear, float zFar)
{
    glm::mat4 newDst(1.0f);

    float f = std::tan(glm::pi<float>() * 0.5f - 0.5f * fovY);

    newDst[0][0] = f / aspect;
    newDst[0][1] = 0;
    newDst[0][2] = 0;
    newDst[0][3] = 0;

    newDst[1][0] = 0;
    newDst[1][1] = f;
    newDst[1][2] = 0;
    newDst[1][3] = 0;

    newDst[2][0] = 0;
    newDst[2][1] = 0;
    newDst[2][3] = -1;

    newDst[3][0] = 0;
    newDst[3][1] = 0;
    newDst[3][3] = 0;

    float rangInv = 1.0f / (zNear - zFar);
    newDst[2][2]  = zFar * rangInv;
    newDst[3][2]  = zFar * zNear * rangInv;

    return newDst;
}

glm::mat4 Camera::LookAt(glm::vec3 eye, glm::vec3 target, glm::vec3 up)
{
    glm::mat4 newDst(1.0f);

    glm::vec3 zAxis = glm::normalize(eye - target);
    glm::vec3 xAxis = glm::normalize(glm::cross(up, zAxis));
    glm::vec3 yAxis = glm::normalize(glm::cross(zAxis, xAxis));

    newDst[0][0] = xAxis[0];
    newDst[0][1] = yAxis[0];
    newDst[0][2] = zAxis[0];
    newDst[0][3] = 0;
    newDst[1][0] = xAxis[1];
    newDst[1][1] = yAxis[1];
    newDst[1][2] = zAxis[1];
    newDst[1][3] = 0;
    newDst[2][0] = xAxis[2];
    newDst[2][1] = yAxis[2];
    newDst[2][2] = zAxis[2];
    newDst[2][3] = 0;

    newDst[3][0] = -(xAxis[0] * eye[0] + xAxis[1] * eye[1] + xAxis[2] * eye[2]);
    newDst[3][1] = -(yAxis[0] * eye[0] + yAxis[1] * eye[1] + yAxis[2] * eye[2]);
    newDst[3][2] = -(zAxis[0] * eye[0] + zAxis[1] * eye[1] + zAxis[2] * eye[2]);
    newDst[3][3] = 1;

    return newDst;
}
