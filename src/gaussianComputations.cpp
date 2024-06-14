#include "gaussianComputations.hpp"
#include <iostream>

//I think this function can be optimized a lot, I am passing and rearranging vector info too many times, need to make it more concise
static Eigen::Matrix<double, 3, 2>  computeUv3DJacobian(const glm::vec3 verticesTriangle3D[3], const glm::vec2 verticesTriangleUV[3]) {
    //3Ds
    const glm::vec3& pos0 = verticesTriangle3D[0];
    const glm::vec3& pos1 = verticesTriangle3D[1];
    const glm::vec3& pos2 = verticesTriangle3D[2];
    //UVs
    const glm::vec2& uv0 = verticesTriangleUV[0];
    const glm::vec2& uv1 = verticesTriangleUV[1];
    const glm::vec2& uv2 = verticesTriangleUV[2];

    Eigen::Vector3d P0(pos0.x, pos0.y, pos0.z), P1(pos1.x, pos1.y, pos1.z), P2(pos2.x, pos2.y, pos2.z);     // 3D positions of the triangle's vertices
    Eigen::Vector2d UV0(uv0.x, uv0.y), UV1(uv1.x, uv1.y), UV2(uv2.x, uv2.y);                                // UV coordinates of the triangle's vertices

    // Compute edge vectors in UV space
    Eigen::Vector2d edgeUV1 = UV1 - UV0;
    Eigen::Vector2d edgeUV2 = UV2 - UV0;

    // Compute edge vectors in 3D space
    Eigen::Vector3d edge3D1 = P1 - P0;
    Eigen::Vector3d edge3D2 = P2 - P0;

    Eigen::Matrix2d UVMatrix;
    UVMatrix <<
        edgeUV1[0], edgeUV2[0],
        edgeUV1[1], edgeUV2[1];

    Eigen::Matrix<double, 3, 2> VMatrix;
    VMatrix <<
        edge3D1[0], edge3D2[0],
        edge3D1[1], edge3D2[1],
        edge3D1[2], edge3D2[2];

    return VMatrix * UVMatrix.inverse(); //This is the Jacobian matrix
}

std::vector<std::pair<glm::vec3, float>> getSortedEigenvectorEigenvalues(Eigen::Matrix3d covMat3d)
{
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eigensolver(covMat3d);

    Eigen::Vector3d eigenvalues = eigensolver.eigenvalues();
    Eigen::Matrix3d eigenvectors = eigensolver.eigenvectors();

    std::vector<std::pair<glm::vec3, float>> eigenPairs;

    for (int i = 0; i < eigenvalues.size(); i++) {
        float eigenvalue = eigenvalues[i];
        glm::vec3 eigenvector(eigenvectors(0, i), eigenvectors(1, i), eigenvectors(2, i));
        eigenPairs.push_back(std::make_pair(eigenvector, eigenvalue));
    }

    //sorted in ascending order, largest eigenvector is at the end with its eigenvalue
    std::sort(eigenPairs.begin(), eigenPairs.end(),
        [](const std::pair<glm::vec3, float>& a, const std::pair<glm::vec3, float>& b) {
            return a.second < b.second;
        }
    );

    return eigenPairs;
}

static glm::vec3 project(const glm::vec3& v, const glm::vec3& u) {
    float scalar = glm::dot(v, u) / glm::dot(u, u);
    return scalar * u;
}

void get3DGaussianQuaternionRotation(const glm::vec3* verticesTriangle3D, glm::vec4& outputQuaternion)
{
    //https://arxiv.org/pdf/2402.01459.pdf
    glm::vec3 r1 = glm::normalize(glm::cross((verticesTriangle3D[1] - verticesTriangle3D[0]), (verticesTriangle3D[2] - verticesTriangle3D[0])));
    glm::vec3 m = (verticesTriangle3D[0] + verticesTriangle3D[1] + verticesTriangle3D[2]) / 3.0f;
    glm::vec3 r2 = glm::normalize(verticesTriangle3D[0] - m);
    //To obtain r3 we first get it as triangleVertices[1] - m and then orthonormalize it with respect to r1 and r2
    glm::vec3 initial_r3 = verticesTriangle3D[1] - m; //DO NOT NORMALIZE THIS
    // Gram-Schmidt Orthonormalization to find r3
    // Project initial_r3 onto r1
    // Project initial_r3 onto r1 and subtract it
    glm::vec3 proj_r3_onto_r1 = project(initial_r3, r1);
    glm::vec3 orthogonal_to_r1 = initial_r3 - proj_r3_onto_r1;

    // Project the result onto r2 and subtract it
    glm::vec3 proj_r3_onto_r2 = project(orthogonal_to_r1, r2);
    glm::vec3 orthogonal_to_r1_and_r2 = orthogonal_to_r1 - proj_r3_onto_r2;

    // Normalize the final result
    glm::vec3 r3 = glm::normalize(orthogonal_to_r1_and_r2);

    glm::mat3 rotMat = { r2, r3, r1 };

    glm::quat q = glm::quat_cast(rotMat);

    outputQuaternion = glm::vec4(q.w, q.x, q.y, q.z);
}

void set3DGaussianScale(const float Sd_x, const float Sd_y, const glm::vec3* verticesTriangle3D, const glm::vec2* verticesTriangleUVs, glm::vec3& outputScale)
{
    //Building CovMat2D
    //float rho = 0.0f; //Pearson Corr. Coeff. (PCC)
    //float covariance = rho * abs(sigma2d) * abs(sigma2d);
    float covariance = 0.0f;
    Eigen::Matrix2d covMat2d;
    float sigmaX = Sd_x * Sd_x;
    float sigmaY = Sd_y * Sd_y;

    covMat2d <<
        sigmaX, covariance,         //Row 1
        covariance, sigmaY;         //Row 2

    //---------------Computing 3d Cov Matrix--------------------
    /*
    std::array<glm::vec3, 3> verticesTriangle3DArray = {
        verticesTriangle3D[0],
        verticesTriangle3D[1],
        verticesTriangle3D[2]
    };

    std::array<glm::vec2, 3> verticesTriangleUVsArray = {
        verticesTriangleUVs[0],
        verticesTriangleUVs[1],
        verticesTriangleUVs[2]
    };
    */

    glm::vec2 triangle2D[3] = {
    glm::vec2(0.0f, 0.0f),
    glm::vec2(Sd_x, 0.0f),
    glm::vec2(0.0f, Sd_y)
    };
    

    auto computeBarycentric2D = [](const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) -> glm::vec3 {
        glm::vec2 v0 = b - a, v1 = c - a, v2 = p - a;
        float d00 = glm::dot(v0, v0);
        float d01 = glm::dot(v0, v1);
        float d11 = glm::dot(v1, v1);
        float d20 = glm::dot(v2, v0);
        float d21 = glm::dot(v2, v1);
        float inv_denom = 1 / (d00 * d11 - d01 * d01);
        float v = (d11 * d20 - d01 * d21) * inv_denom;
        float w = (d00 * d21 - d01 * d20) * inv_denom;
        float u = 1.0f - v - w;
        return glm::vec3(u, v, w);
        };

    glm::vec3 baryCoords2D[3];
    for (int i = 0; i < 3; ++i)
    {
        baryCoords2D[i] = computeBarycentric2D(triangle2D[i], verticesTriangleUVs[0], verticesTriangleUVs[1], verticesTriangleUVs[2]);
    }

    glm::vec3 newTriangle3D[3];
    for (int i = 0; i < 3; ++i)
    {
        newTriangle3D[i] = 
            baryCoords2D[i].x * verticesTriangle3D[0] +
            baryCoords2D[i].y * verticesTriangle3D[1] +
            baryCoords2D[i].z * verticesTriangle3D[2];
    }

    Eigen::Matrix<double, 3, 2> J = computeUv3DJacobian(newTriangle3D, triangle2D);

    Eigen::Matrix3d covMat3d = J * covMat2d * J.transpose();

    std::vector<std::pair<glm::vec3, float>> eigenPairs = getSortedEigenvectorEigenvalues(covMat3d);

    //Computing and setting scale values from the eigenvalues
    float SD_xy = sqrtf(eigenPairs[2].second); //3 sigma, need to divide by 3 in the renderer, I select the second largest value, not the largest
    //float SD_y = Sd_x;//0.3333f * sqrtf(eigenPairs[1].second);
    float SD_z = EPSILON; //SD_xy / 5.0f;//(std::min(SD_x, SD_y) / 5.0f); //TODO: magic hyperparameter needs to be understood better

    outputScale = glm::vec3(log(SD_xy), log(SD_xy), log(SD_z));
}