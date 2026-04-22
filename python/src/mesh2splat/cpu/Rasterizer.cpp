///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - CPU Rasterizer Implementation       //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "Rasterizer.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mesh2splat {

Rasterizer::Rasterizer(int resolution, RasterizationMode mode) 
    : resolution_(resolution), mode_(mode) {}

glm::mat2x3 Rasterizer::computeUv3DJacobian(const std::array<glm::vec3, 3>& positions,
                                             const std::array<glm::vec2, 3>& uvs) {
    // UV matrix
    glm::mat2 uvMatrix;
    uvMatrix[0][0] = uvs[1].x - uvs[0].x;
    uvMatrix[1][0] = uvs[2].x - uvs[0].x;
    uvMatrix[0][1] = uvs[1].y - uvs[0].y;
    uvMatrix[1][1] = uvs[2].y - uvs[0].y;
    
    // Vertex position matrix
    glm::mat2x3 vMatrix;
    vMatrix[0][0] = positions[1].x - positions[0].x;
    vMatrix[1][0] = positions[2].x - positions[0].x;
    vMatrix[0][1] = positions[1].y - positions[0].y;
    vMatrix[1][1] = positions[2].y - positions[0].y;
    vMatrix[0][2] = positions[1].z - positions[0].z;
    vMatrix[1][2] = positions[2].z - positions[0].z;
    
    // Compute inverse of UV matrix
    float det = uvMatrix[0][0] * uvMatrix[1][1] - uvMatrix[0][1] * uvMatrix[1][0];
    if (std::abs(det) < epsilon::kDeterminant) {
        det = 1.0f;
    }
    float invDet = 1.0f / det;
    
    glm::mat2 uvInv;
    uvInv[0][0] = uvMatrix[1][1] * invDet;
    uvInv[1][0] = -uvMatrix[1][0] * invDet;
    uvInv[0][1] = -uvMatrix[0][1] * invDet;
    uvInv[1][1] = uvMatrix[0][0] * invDet;
    
    // J = V * UV^-1
    glm::mat2x3 J;
    J[0][0] = vMatrix[0][0] * uvInv[0][0] + vMatrix[1][0] * uvInv[0][1];
    J[1][0] = vMatrix[0][0] * uvInv[1][0] + vMatrix[1][0] * uvInv[1][1];
    J[0][1] = vMatrix[0][1] * uvInv[0][0] + vMatrix[1][1] * uvInv[0][1];
    J[1][1] = vMatrix[0][1] * uvInv[1][0] + vMatrix[1][1] * uvInv[1][1];
    J[0][2] = vMatrix[0][2] * uvInv[0][0] + vMatrix[1][2] * uvInv[0][1];
    J[1][2] = vMatrix[0][2] * uvInv[1][0] + vMatrix[1][2] * uvInv[1][1];
    
    return J;
}

glm::quat Rasterizer::matrixToQuaternion(const glm::mat3& m) {
    // Direct port from converterGS.glsl quat_cast
    float fourXSquaredMinus1 = m[0][0] - m[1][1] - m[2][2];
    float fourYSquaredMinus1 = m[1][1] - m[0][0] - m[2][2];
    float fourZSquaredMinus1 = m[2][2] - m[0][0] - m[1][1];
    float fourWSquaredMinus1 = m[0][0] + m[1][1] + m[2][2];
    
    int biggestIndex = 0;
    float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourXSquaredMinus1;
        biggestIndex = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourYSquaredMinus1;
        biggestIndex = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourZSquaredMinus1;
        biggestIndex = 3;
    }
    
    float biggestVal = std::sqrt(std::max(fourBiggestSquaredMinus1 + 1.0f, epsilon::kQuatSqrt)) * 0.5f;
    float mult = 0.25f / biggestVal;
    
    glm::quat q;
    
    if (biggestIndex == 0) {
        q.w = biggestVal;
        q.x = (m[1][2] - m[2][1]) * mult;
        q.y = (m[2][0] - m[0][2]) * mult;
        q.z = (m[0][1] - m[1][0]) * mult;
    } else if (biggestIndex == 1) {
        q.w = (m[1][2] - m[2][1]) * mult;
        q.x = biggestVal;
        q.y = (m[0][1] + m[1][0]) * mult;
        q.z = (m[2][0] + m[0][2]) * mult;
    } else if (biggestIndex == 2) {
        q.w = (m[2][0] - m[0][2]) * mult;
        q.x = (m[0][1] + m[1][0]) * mult;
        q.y = biggestVal;
        q.z = (m[1][2] + m[2][1]) * mult;
    } else {
        q.w = (m[0][1] - m[1][0]) * mult;
        q.x = (m[2][0] + m[0][2]) * mult;
        q.y = (m[1][2] + m[2][1]) * mult;
        q.z = biggestVal;
    }
    
    return glm::normalize(q);
}

void Rasterizer::computeFaceDataProjection(const Face& face, const BBox& bbox,
                                            glm::vec3& outScale, glm::vec4& outRotation,
                                            std::array<glm::vec2, 3>& outRasterUvs) {
    // Port of converterGS.glsl main()
    
    // Compute edges
    glm::vec3 edge1 = face.positions[1] - face.positions[0];
    glm::vec3 edge2 = face.positions[2] - face.positions[0];
    glm::vec3 edge3 = face.positions[2] - face.positions[1];
    
    // Check for degenerate triangle (cross product would be zero/near-zero)
    glm::vec3 cross = glm::cross(edge1, edge2);
    float crossLen = glm::length(cross);
    if (crossLen < epsilon::kDegenerate) {
        // Degenerate triangle - return safe defaults
        outScale = glm::vec3(epsilon::kMinScale);
        outRotation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);  // identity quaternion (w,x,y,z)
        for (int i = 0; i < 3; i++) {
            outRasterUvs[i] = glm::vec2(0.0f);
        }
        return;
    }
    
    // H14 fix: compute face normal from original edges BEFORE swapping
    glm::vec3 normal = glm::normalize(cross);
    
    // Find longest edge
    if (glm::length(edge2) > glm::length(edge1) && glm::length(edge2) > glm::length(edge3)) {
        std::swap(edge1, edge2);
    } else if (glm::length(edge3) > glm::length(edge1) && glm::length(edge3) > glm::length(edge2)) {
        std::swap(edge1, edge3);
    }
    
    // Normalize longest edge
    edge1 = glm::normalize(edge1);
    
    float absX = std::abs(normal.x);
    float absY = std::abs(normal.y);
    float absZ = std::abs(normal.z);
    
    // Compute orthogonal UVs based on dominant normal direction
    for (int i = 0; i < 3; i++) {
        float u, v;
        glm::vec3 pos = face.positions[i];
        
        if (absX > absY && absX > absZ) {
            float rangeY = bbox.max.y - bbox.min.y;
            float rangeZ = bbox.max.z - bbox.min.z;
            float range = std::max(rangeY, rangeZ);
            if (range < epsilon::kRange) range = 1.0f;
            
            float relY = pos.y - bbox.min.y;
            float relZ = pos.z - bbox.min.z;
            
            u = relY / range;
            v = relZ / range;
        } else if (absY > absZ) {
            float rangeX = bbox.max.x - bbox.min.x;
            float rangeZ = bbox.max.z - bbox.min.z;
            float range = std::max(rangeX, rangeZ);
            if (range < epsilon::kRange) range = 1.0f;
            
            float relX = pos.x - bbox.min.x;
            float relZ = pos.z - bbox.min.z;
            
            u = relX / range;
            v = relZ / range;
        } else {
            float rangeX = bbox.max.x - bbox.min.x;
            float rangeY = bbox.max.y - bbox.min.y;
            float range = std::max(rangeX, rangeY);
            if (range < epsilon::kRange) range = 1.0f;
            
            float relX = pos.x - bbox.min.x;
            float relY = pos.y - bbox.min.y;
            
            u = relX / range;
            v = relY / range;
        }
        
        outRasterUvs[i] = glm::vec2(u, v);
    }
    
    // Compute rotation matrix from triangle orientation
    glm::vec3 xAxis = edge1;
    glm::vec3 yAxis = glm::normalize(glm::cross(normal, xAxis));
    glm::vec3 zAxis = normal;
    
    glm::mat3 rotationMatrix(xAxis, yAxis, zAxis);
    glm::quat q = matrixToQuaternion(rotationMatrix);
    
    // Output quaternion as (w, x, y, z) format matching shader
    outRotation = glm::vec4(q.w, q.x, q.y, q.z);
    
    // Compute Jacobian to get gaussian scale
    glm::mat2x3 J = computeUv3DJacobian(face.positions, outRasterUvs);
    
    // Transpose J to get Ju and Jv
    glm::vec3 Ju(J[0][0], J[0][1], J[0][2]);
    glm::vec3 Jv(J[1][0], J[1][1], J[1][2]);
    
    float gaussianScaleX = glm::length(Ju);
    float gaussianScaleY = glm::length(Jv);
    
    outScale = glm::vec3(gaussianScaleX, gaussianScaleY, epsilon::kMinScale);
}

void Rasterizer::computeFaceDataUV(const Face& face,
                                    glm::vec3& outScale, glm::vec4& outRotation,
                                    std::array<glm::vec2, 3>& outRasterUvs) {
    // UV mode: use the original mesh UVs for rasterization
    // This produces gaussians that align with the texture atlas
    
    // Use original UVs directly for rasterization
    for (int i = 0; i < 3; i++) {
        outRasterUvs[i] = face.uvs[i];
    }
    
    // Compute edges for orientation
    glm::vec3 edge1 = face.positions[1] - face.positions[0];
    glm::vec3 edge2 = face.positions[2] - face.positions[0];
    glm::vec3 edge3 = face.positions[2] - face.positions[1];
    
    // Check for degenerate triangle (cross product would be zero/near-zero)
    glm::vec3 cross = glm::cross(edge1, edge2);
    float crossLen = glm::length(cross);
    if (crossLen < epsilon::kDegenerate) {
        // Degenerate triangle - return safe defaults
        outScale = glm::vec3(epsilon::kMinScale);
        outRotation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);  // identity quaternion (w,x,y,z)
        return;
    }
    
    // H14 fix: compute face normal from original edges BEFORE swapping
    glm::vec3 normal = glm::normalize(cross);
    
    // Find longest edge for consistent orientation (before computing normal)
    if (glm::length(edge2) > glm::length(edge1) && glm::length(edge2) > glm::length(edge3)) {
        std::swap(edge1, edge2);
    } else if (glm::length(edge3) > glm::length(edge1) && glm::length(edge3) > glm::length(edge2)) {
        std::swap(edge1, edge3);
    }
    
    // Normalize longest edge
    edge1 = glm::normalize(edge1);
    
    // Compute rotation matrix from triangle orientation
    glm::vec3 xAxis = edge1;
    glm::vec3 yAxis = glm::normalize(glm::cross(normal, xAxis));
    glm::vec3 zAxis = normal;
    
    glm::mat3 rotationMatrix(xAxis, yAxis, zAxis);
    glm::quat q = matrixToQuaternion(rotationMatrix);
    
    // Output quaternion as (w, x, y, z) format
    outRotation = glm::vec4(q.w, q.x, q.y, q.z);
    
    // Compute Jacobian from original UVs to get gaussian scale
    // This maps UV space deltas to 3D space deltas
    glm::mat2x3 J = computeUv3DJacobian(face.positions, face.uvs);
    
    // Extract scale from Jacobian columns
    glm::vec3 Ju(J[0][0], J[0][1], J[0][2]);
    glm::vec3 Jv(J[1][0], J[1][1], J[1][2]);
    
    float gaussianScaleX = glm::length(Ju);
    float gaussianScaleY = glm::length(Jv);
    
    outScale = glm::vec3(gaussianScaleX, gaussianScaleY, epsilon::kMinScale);
}

void Rasterizer::rasterizeTriangle(const Face& face, const Material& material,
                                    const glm::vec3& scale, const glm::vec4& rotation,
                                    const std::array<glm::vec2, 3>& rasterUvs,
                                    const FragmentCallback& callback) {
    // Convert raster UVs to pixel coordinates
    std::array<glm::vec2, 3> pixelCoords;
    for (int i = 0; i < 3; i++) {
        pixelCoords[i] = rasterUvs[i] * static_cast<float>(resolution_ - 1);
    }
    
    // Compute bounding box in pixel space
    glm::vec2 minPx(std::numeric_limits<float>::max());
    glm::vec2 maxPx(std::numeric_limits<float>::lowest());
    
    for (int i = 0; i < 3; i++) {
        minPx = glm::min(minPx, pixelCoords[i]);
        maxPx = glm::max(maxPx, pixelCoords[i]);
    }
    
    // Clamp to resolution
    int minX = std::max(0, static_cast<int>(std::floor(minPx.x)));
    int maxX = std::min(resolution_ - 1, static_cast<int>(std::ceil(maxPx.x)));
    int minY = std::max(0, static_cast<int>(std::floor(minPx.y)));
    int maxY = std::min(resolution_ - 1, static_cast<int>(std::ceil(maxPx.y)));
    
    // Rasterize pixels within bounding box
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            glm::vec2 p(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
            
            // Compute barycentric coordinates
            float u, v, w;
            if (!computeBarycentric(p, pixelCoords[0], pixelCoords[1], pixelCoords[2], u, v, w)) {
                continue;
            }
            
            // Check if inside triangle
            if (u < 0.0f || v < 0.0f || w < 0.0f) {
                continue;
            }
            
            // Interpolate vertex attributes
            RasterFragment frag;
            frag.valid = true;
            
            frag.position = u * face.positions[0] + v * face.positions[1] + w * face.positions[2];
            frag.normal = glm::normalize(u * face.normals[0] + v * face.normals[1] + w * face.normals[2]);
            frag.tangent = u * face.tangents[0] + v * face.tangents[1] + w * face.tangents[2];
            frag.uv = u * face.uvs[0] + v * face.uvs[1] + w * face.uvs[2];
            frag.scale = scale;
            frag.rotation = rotation;
            
            callback(frag, material);
        }
    }
}

void Rasterizer::rasterize(const Mesh& mesh, const FragmentCallback& callback) {
    for (const auto& face : mesh.faces) {
        glm::vec3 scale;
        glm::vec4 rotation;
        std::array<glm::vec2, 3> rasterUvs;
        
        if (mode_ == RasterizationMode::UV) {
            computeFaceDataUV(face, scale, rotation, rasterUvs);
        } else {
            computeFaceDataProjection(face, mesh.bbox, scale, rotation, rasterUvs);
        }
        rasterizeTriangle(face, mesh.material, scale, rotation, rasterUvs, callback);
    }
}

void Rasterizer::rasterize(const Scene& scene, const FragmentCallback& callback) {
    for (const auto& mesh : scene.meshes) {
        rasterize(mesh, callback);
    }
}

} // namespace mesh2splat
