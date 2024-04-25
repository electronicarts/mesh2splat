#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/hash.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/string_cast.hpp>

#include <unordered_set>
#include <Eigen/Dense>
#include <Eigen/Geometry>

static Eigen::Matrix<double, 3, 2>  computeUv3DJacobian(const std::array<glm::vec3, 3> verticesTriangle3D, const std::array<glm::vec2, 3> verticesTriangleUV);

std::vector<std::pair<glm::vec3, float>> getSortedEigenvectorEigenvalues(Eigen::Matrix3d covMat3d);

std::pair<glm::vec4, glm::vec3> getScaleRotationGaussian(const float sigma2d, std::vector<glm::vec3> verticesTriangle3D, std::vector<glm::vec2> verticesTriangleUVs);