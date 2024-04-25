#include "gaussianShapesUtilities.hpp"

std::vector<Gaussian3D> drawLine(glm::vec3 initialPos, glm::vec3 finalPos, glm::vec3 color, float isotropicScale, float opacity = 1.0f)
{
    std::vector<glm::vec3> interpolatedPositions;
    for (int i = 0; i < 30; i++)
    {
        float alpha = ((float)i) / 100.0f;
        interpolatedPositions.push_back(floatToVec3(1 - alpha) * initialPos + floatToVec3(alpha) * finalPos);
    }

    std::vector<Gaussian3D> normalGaussians;

    for (glm::vec3& position : interpolatedPositions)
    {
        normalGaussians.push_back(
            Gaussian3D(
                position,
                glm::vec3(0.0f, 0.0f, 1.0f), //Not used anyway
                log(glm::vec3(isotropicScale, isotropicScale, isotropicScale)),
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), //Gaussian is isotropic so not used
                getColor(color),
                1.0f,
                MaterialGltf()
            )
        );
    }
    return normalGaussians;
}

std::vector<Gaussian3D> createEncompassingTriangle(std::array<glm::vec3, 3> positions, glm::vec3 color, float isotropicScale, float opacity = 1.0f)
{
    std::vector<Gaussian3D> triangleGaussians;

    for (glm::vec3& position : positions)
    {
        triangleGaussians.push_back(
            Gaussian3D(
                position,
                glm::vec3(0.0f, 0.0f, 1.0f), //Not used anyway
                log(glm::vec3(isotropicScale, isotropicScale, isotropicScale)),
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), //Gaussian is isotropic so not used
                color,
                1.0f,
                MaterialGltf()
            )
        );
    }
    return triangleGaussians;
}



//Expects to receive 24 positions, 4 per each of the 6 faces
std::vector<Gaussian3D> drawCube(std::vector<glm::vec3> positions, glm::vec3 color, float isotropicScale, float opacity = 1.0f)
{
    std::vector<std::vector<glm::vec3>> faces;
    for (int i = 0; i < 6; i++)
    {
        faces.push_back(std::vector<glm::vec3>(positions.begin() + i * 4, positions.end() + (i * 4) + 4));
    }

    std::vector<Gaussian3D> cubeEdges;

    for (std::vector<glm::vec3> face : faces)
    {
        for (int i = 0; i < 4; i++)
        {
            auto edge = drawLine(face[i], face[(i + 1) % 4], color, isotropicScale, opacity);
            cubeEdges.insert(cubeEdges.end(), edge.begin(), edge.end());
        }

    }

    return cubeEdges;
}

void addNormalVector3DGaussianRepresentation(std::vector<glm::vec3> triangleVertices, glm::vec3 normal, std::vector<Gaussian3D>& gaussians_3D_list)
{
    glm::vec3 center = (1.0f / 3.0f) * (triangleVertices[0] + triangleVertices[1] + triangleVertices[2]);
    std::vector<Gaussian3D> normalLine = drawLine(center, center + (normal * 5.0f), glm::vec3(0.294f, 0.706f, 0.89f), 0.01f);
    gaussians_3D_list.insert(gaussians_3D_list.end(), normalLine.begin(), normalLine.end());
}