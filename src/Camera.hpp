#ifndef CAMERA_HPP
#define CAMERA_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class Camera
{
private:
    glm::mat4 projectionMatrix { 1.f };
    glm::mat4 viewMatrix { 1.f };
public:
    void setOrthographicProjection(const float left, const float right, const float top, const float bottom, const float near, const float far);
    void setPerspectiveProjection(const float fovy, const float aspect, const float near, const float far);

    void setViewDirection(const glm::vec3 position, const glm::vec3 direction, const glm::vec3 up = glm::vec3(0.f, -1.f, 0.f));
    void setViewTarget(const glm::vec3 position, const glm::vec3 target, const glm::vec3 up = glm::vec3(0.f, -1.f, 0.f));
    void setViewYXZ(const glm::vec3 position, const glm::vec3 rotation);

    const glm::mat4& getProjection() const { return projectionMatrix; }
    const glm::mat4& getView() const { return viewMatrix; }
};

#endif // CAMERA_HPP