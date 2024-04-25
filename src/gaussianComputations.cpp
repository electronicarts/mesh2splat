#include "gaussianComputations.hpp"

//I think this function can be optimized a lot, I am passing and rearranging vector info too many times, need to make it more concise
static Eigen::Matrix<double, 3, 2>  computeUv3DJacobian(const std::array<glm::vec3, 3> verticesTriangle3D, const std::array<glm::vec2, 3> verticesTriangleUV) {
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

std::pair<glm::vec4, glm::vec3> getScaleRotationGaussian(const float sigma2d, std::vector<glm::vec3> verticesTriangle3D, std::vector<glm::vec2> verticesTriangleUVs)
{
    //Building CovMat2D
    //float rho = 0.0f; //Pearson Corr. Coeff. (PCC)
    //float covariance = rho * abs(sigma2d) * abs(sigma2d);
    float covariance = 0.0f;
    Eigen::Matrix2d covMat2d;
    float sigma2 = sigma2d * sigma2d;
    covMat2d <<
        sigma2, covariance,     //Row 1
        covariance, sigma2;         //Row 2

    //---------------Computing 3d Cov Matrix--------------------

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

    Eigen::Matrix<double, 3, 2> J = computeUv3DJacobian(verticesTriangle3DArray, verticesTriangleUVsArray);

    Eigen::Matrix3d covMat3d = J * covMat2d * J.transpose();

    std::vector<std::pair<glm::vec3, float>> eigenPairs = getSortedEigenvectorEigenvalues(covMat3d);

    //Computing and setting scale values from the eigenvalues
    float SD_x = sqrt(eigenPairs[2].second); //sqrt of eigenvalues..., jacobian by normal dot should yield 0 (to check)
    float SD_y = sqrt(eigenPairs[1].second);
    float SD_z = (std::min(SD_x, SD_y) / 5.0f);

    glm::vec3 x = glm::normalize(eigenPairs[2].first);
    glm::vec3 y = glm::normalize(eigenPairs[1].first);
    glm::vec3 z = glm::normalize(glm::cross(x, y));

    //TODO: for now I do this. I know it would make sense to directly use the computed normal. 
    // but I dont know if they are exactly identical and do not want to risk having a non-orthonormal basis + orthonormalizing it may add overhead.

    glm::mat3 rotMat = { x, y, z };

    glm::quat q = glm::quat_cast(rotMat);

    return std::make_pair(glm::vec4(q.w, q.x, q.y, q.z), glm::vec3(log(SD_x), log(SD_y), log(SD_z)));
}