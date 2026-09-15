///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

// Coordinate-convention helpers for splat export.
//
// Mesh2Splat performs no handedness or axis remapping during conversion: a
// gaussian ends up at exactly the glTF world-space position of the surface it
// was sampled from (node transforms applied, see SceneManager::loadModel). The
// exported PLY therefore inherits the glTF/OpenGL convention:
//
//     right-handed, +X right, +Y up, +Z towards the viewer
//
// The reference 3D gaussian splatting implementation and every tool built on
// COLMAP poses instead expect world space in the COLMAP/OpenCV convention:
//
//     right-handed, +X right, +Y down, +Z forward (into the scene)
//
// Both frames are right-handed, so they differ by a proper rotation of 180
// degrees about the X axis, diag(1, -1, -1). Feeding COLMAP-style extrinsics to
// a splat exported in the glTF frame puts the whole scene behind the camera,
// which is why such renders come out empty.
//
// This header is deliberately free of OpenGL/ImGui/tinygltf includes so the
// math can be unit tested on its own (see tests/).

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace utils
{
    // Target world-space convention for the exported splat.
    // The values are stable: they are persisted in the UI and CLI surface.
    enum class CoordinateSystem : unsigned int
    {
        // Leave positions exactly as the source glTF defined them. Default, and
        // what every Mesh2Splat release before this option produced.
        GltfYUp     = 0,

        // Rotate into the COLMAP/OpenCV frame used by the reference 3DGS
        // rasterizer, Nerfstudio, gsplat and friends.
        ColmapYDown = 1,
    };

    inline const char* toString(CoordinateSystem cs)
    {
        switch (cs)
        {
            case CoordinateSystem::GltfYUp:     return "glTF / OpenGL (+Y up)";
            case CoordinateSystem::ColmapYDown: return "COLMAP / OpenCV (+Y down)";
        }
        return "unknown";
    }

    // Rotation taking a vector from the glTF world frame into `cs`.
    //
    // diag(1, -1, -1) has determinant +1, so this is a rotation and not a
    // mirror: handedness, winding and normal orientation are all preserved. It
    // is also an involution (R * R == I), so the same matrix converts back.
    inline glm::mat3 gltfToCoordinateSystem(CoordinateSystem cs)
    {
        switch (cs)
        {
            case CoordinateSystem::ColmapYDown:
                return glm::mat3(
                    1.0f,  0.0f,  0.0f,
                    0.0f, -1.0f,  0.0f,
                    0.0f,  0.0f, -1.0f
                );
            case CoordinateSystem::GltfYUp:
            default:
                return glm::mat3(1.0f);
        }
    }

    // Same rotation expressed as a quaternion, scalar-first (w, x, y, z).
    // 180 degrees about X is (w=0, x=1, y=0, z=0).
    inline glm::quat gltfToCoordinateSystemQuat(CoordinateSystem cs)
    {
        switch (cs)
        {
            case CoordinateSystem::ColmapYDown:
                return glm::quat(0.0f, 1.0f, 0.0f, 0.0f);
            case CoordinateSystem::GltfYUp:
            default:
                return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    inline glm::vec3 convertPosition(const glm::vec3& positionGltf, CoordinateSystem cs)
    {
        return gltfToCoordinateSystem(cs) * positionGltf;
    }

    // Normals and other directions rotate by the same matrix: for a pure
    // rotation the normal matrix transpose(inverse(R)) is R itself.
    inline glm::vec3 convertDirection(const glm::vec3& directionGltf, CoordinateSystem cs)
    {
        return gltfToCoordinateSystem(cs) * directionGltf;
    }

    // Rotate a gaussian's orientation quaternion into `cs`.
    //
    // `rotationWxyz` is packed the way Mesh2Splat stores it everywhere else: the
    // vec4 components (.x, .y, .z, .w) hold (w, x, y, z), matching the PLY
    // property order rot_0..rot_3 used by the reference 3DGS format. Getting
    // this ordering wrong is the classic source of silently mangled splats, so
    // the packing is converted explicitly rather than reinterpreted.
    //
    // The gaussian quaternion q_g maps local axes into world space, so pre-
    // multiplying by the frame rotation gives the orientation in the new frame.
    // The covariance follows: R (R_g S S^T R_g^T) R^T == (R R_g) S S^T (R R_g)^T,
    // which is why scales need no adjustment.
    inline glm::vec4 convertRotationWxyz(const glm::vec4& rotationWxyz, CoordinateSystem cs)
    {
        if (cs == CoordinateSystem::GltfYUp) return rotationWxyz;

        const glm::quat gaussian(rotationWxyz.x, rotationWxyz.y, rotationWxyz.z, rotationWxyz.w);
        const glm::quat rotated = glm::normalize(gltfToCoordinateSystemQuat(cs) * gaussian);

        return glm::vec4(rotated.w, rotated.x, rotated.y, rotated.z);
    }

    // Rotate a single gaussian in place.
    //
    // Templated on the gaussian type so the conversion can be exercised by the
    // unit tests without dragging in the OpenGL-facing utils.hpp. Any type with
    // vec4 `position`, `normal` and `rotation` members works, which is exactly
    // the layout of utils::GaussianDataSSBO.
    //
    // `scale` is intentionally untouched: the frame change is a pure rotation,
    // so it is absorbed by the rotation term of the covariance.
    template <typename GaussianT>
    inline void convertGaussianInPlace(GaussianT& gaussian, CoordinateSystem cs)
    {
        if (cs == CoordinateSystem::GltfYUp) return;

        const glm::mat3 R = gltfToCoordinateSystem(cs);

        const glm::vec3 position = R * glm::vec3(gaussian.position);
        gaussian.position = glm::vec4(position, gaussian.position.w);

        const glm::vec3 normal = R * glm::vec3(gaussian.normal);
        gaussian.normal = glm::vec4(normal, gaussian.normal.w);

        gaussian.rotation = convertRotationWxyz(gaussian.rotation, cs);
    }

    template <typename GaussianContainerT>
    inline void convertGaussiansInPlace(GaussianContainerT& gaussians, CoordinateSystem cs)
    {
        if (cs == CoordinateSystem::GltfYUp) return;

        for (auto& gaussian : gaussians) convertGaussianInPlace(gaussian, cs);
    }
}
