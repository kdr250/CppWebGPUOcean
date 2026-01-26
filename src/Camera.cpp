#include "Camera.h"
#include <glm/gtc/constants.hpp>
#include <glm/ext.hpp>
#include <iostream>

#include "Application.h"

Camera::Camera()
{
    // TODO
}

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
    renderUniforms.invProjectionMatrix = Inverse(projection);

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

    std::cout << "cameraPosition = { " << cameraPosition.x << ", " << cameraPosition.y << ", "
              << cameraPosition.z << " }" << std::endl;
    std::cout << "target = { " << target.x << ", " << target.y << ", " << target.z << " }"
              << std::endl;

    auto view = LookAt(cameraPosition, this->target, glm::vec3(0.0f, 1.0f, 0.0f));

    renderUniforms.viewMatrix    = view;
    renderUniforms.invViewMatrix = Inverse(view);
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

glm::mat4 Camera::Inverse(glm::mat4 m)
{
    glm::mat4 newDst(1.0f);

    float m00 = m[0][0];
    float m01 = m[0][1];
    float m02 = m[0][2];
    float m03 = m[0][3];
    float m10 = m[1][0];
    float m11 = m[1][1];
    float m12 = m[1][2];
    float m13 = m[1][3];
    float m20 = m[2][0];
    float m21 = m[2][1];
    float m22 = m[2][2];
    float m23 = m[2][3];
    float m30 = m[3][0];
    float m31 = m[3][1];
    float m32 = m[3][2];
    float m33 = m[3][3];

    float tmp0  = m22 * m33;
    float tmp1  = m32 * m23;
    float tmp2  = m12 * m33;
    float tmp3  = m32 * m13;
    float tmp4  = m12 * m23;
    float tmp5  = m22 * m13;
    float tmp6  = m02 * m33;
    float tmp7  = m32 * m03;
    float tmp8  = m02 * m23;
    float tmp9  = m22 * m03;
    float tmp10 = m02 * m13;
    float tmp11 = m12 * m03;
    float tmp12 = m20 * m31;
    float tmp13 = m30 * m21;
    float tmp14 = m10 * m31;
    float tmp15 = m30 * m11;
    float tmp16 = m10 * m21;
    float tmp17 = m20 * m11;
    float tmp18 = m00 * m31;
    float tmp19 = m30 * m01;
    float tmp20 = m00 * m21;
    float tmp21 = m20 * m01;
    float tmp22 = m00 * m11;
    float tmp23 = m10 * m01;

    float t0 = (tmp0 * m11 + tmp3 * m21 + tmp4 * m31) - (tmp1 * m11 + tmp2 * m21 + tmp5 * m31);
    float t1 = (tmp1 * m01 + tmp6 * m21 + tmp9 * m31) - (tmp0 * m01 + tmp7 * m21 + tmp8 * m31);
    float t2 = (tmp2 * m01 + tmp7 * m11 + tmp10 * m31) - (tmp3 * m01 + tmp6 * m11 + tmp11 * m31);
    float t3 = (tmp5 * m01 + tmp8 * m11 + tmp11 * m21) - (tmp4 * m01 + tmp9 * m11 + tmp10 * m21);

    float d = 1 / (m00 * t0 + m10 * t1 + m20 * t2 + m30 * t3);

    newDst[0][0] = d * t0;
    newDst[0][1] = d * t1;
    newDst[0][2] = d * t2;
    newDst[0][3] = d * t3;

    newDst[1][0] =
        d * ((tmp1 * m10 + tmp2 * m20 + tmp5 * m30) - (tmp0 * m10 + tmp3 * m20 + tmp4 * m30));
    newDst[1][1] =
        d * ((tmp0 * m00 + tmp7 * m20 + tmp8 * m30) - (tmp1 * m00 + tmp6 * m20 + tmp9 * m30));
    newDst[1][2] =
        d * ((tmp3 * m00 + tmp6 * m10 + tmp11 * m30) - (tmp2 * m00 + tmp7 * m10 + tmp10 * m30));
    newDst[1][3] =
        d * ((tmp4 * m00 + tmp9 * m10 + tmp10 * m20) - (tmp5 * m00 + tmp8 * m10 + tmp11 * m20));
    newDst[2][0] =
        d * ((tmp12 * m13 + tmp15 * m23 + tmp16 * m33) - (tmp13 * m13 + tmp14 * m23 + tmp17 * m33));
    newDst[2][1] =
        d * ((tmp13 * m03 + tmp18 * m23 + tmp21 * m33) - (tmp12 * m03 + tmp19 * m23 + tmp20 * m33));
    newDst[2][2] =
        d * ((tmp14 * m03 + tmp19 * m13 + tmp22 * m33) - (tmp15 * m03 + tmp18 * m13 + tmp23 * m33));
    newDst[2][3] =
        d * ((tmp17 * m03 + tmp20 * m13 + tmp23 * m23) - (tmp16 * m03 + tmp21 * m13 + tmp22 * m23));
    newDst[3][0] =
        d * ((tmp14 * m22 + tmp17 * m32 + tmp13 * m12) - (tmp16 * m32 + tmp12 * m12 + tmp15 * m22));
    newDst[3][1] =
        d * ((tmp20 * m32 + tmp12 * m02 + tmp19 * m22) - (tmp18 * m22 + tmp21 * m32 + tmp13 * m02));
    newDst[3][2] =
        d * ((tmp18 * m12 + tmp23 * m32 + tmp15 * m02) - (tmp22 * m32 + tmp14 * m02 + tmp19 * m12));
    newDst[3][3] =
        d * ((tmp22 * m22 + tmp16 * m02 + tmp21 * m12) - (tmp20 * m12 + tmp23 * m22 + tmp17 * m02));

    return newDst;
}
