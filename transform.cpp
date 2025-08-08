#include "transform.hpp"

using namespace okami;

Transform Transform::LookAt(glm::vec3 const& eye, glm::vec3 const& target, glm::vec3 const& up) {
    glm::vec3 zaxis = glm::normalize(eye - target);
    glm::vec3 xaxis = glm::normalize(glm::cross(up, zaxis));
    glm::vec3 yaxis = glm::cross(zaxis, xaxis);
    glm::mat3 rotation = glm::mat3(xaxis, yaxis, zaxis);
    return Transform(eye, glm::quat(rotation), 1.0f);
}