#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/hash.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/string_cast.hpp>

#include <unordered_set>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include "utils/params.hpp"

static Eigen::Matrix<double, 3, 2>  computeUv3DJacobian(const std::array<glm::vec3, 3> verticesTriangle3D, const std::array<glm::vec2, 3> verticesTriangleUV);

std::vector<std::pair<glm::vec3, float>> getSortedEigenvectorEigenvalues(Eigen::Matrix3d covMat3d);

void get3DGaussianQuaternionRotation(const glm::vec3* verticesTriangle3D, glm::vec4& outputQuaternion);


void set3DGaussianScale(const float Sd_x, const float Sd_y, const glm::vec3* verticesTriangle3D, const glm::vec2* verticesTriangleUVs, glm::vec3& outputScale);