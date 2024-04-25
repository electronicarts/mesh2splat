#include <vector>
#include <array>
#include "utils.hpp"

std::vector<Gaussian3D> drawLine(glm::vec3 initialPos, glm::vec3 finalPos, glm::vec3 color, float isotropicScale, float opacity);

std::vector<Gaussian3D> createEncompassingTriangle(std::array<glm::vec3, 3> positions, glm::vec3 color, float isotropicScale, float opacity);

std::vector<Gaussian3D> drawCube(std::vector<glm::vec3> positions, glm::vec3 color, float isotropicScale, float opacity);

void addNormalVector3DGaussianRepresentation(std::vector<glm::vec3> triangleVertices, glm::vec3 normal, std::vector<Gaussian3D>& gaussians_3D_list);