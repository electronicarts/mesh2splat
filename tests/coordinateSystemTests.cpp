///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

// Unit tests for the export-time world-convention conversion.
//
// Deliberately dependency free: the only include outside the standard library
// is the vendored, header-only glm. That keeps the suite hermetic (no network
// fetch, no OpenGL context, no window server) so it runs unchanged in CI and in
// a sandbox.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "utils/coordinateSystem.hpp"

namespace
{
    int g_failures = 0;
    int g_checks   = 0;

    constexpr float kEps = 1e-5f;

    void check(bool condition, const std::string& what)
    {
        ++g_checks;
        if (!condition)
        {
            ++g_failures;
            std::printf("  [FAIL] %s\n", what.c_str());
        }
    }

    void checkNear(float a, float b, const std::string& what)
    {
        check(std::fabs(a - b) <= kEps, what + " (got " + std::to_string(a) + ", want " + std::to_string(b) + ")");
    }

    void checkNear(const glm::vec3& a, const glm::vec3& b, const std::string& what)
    {
        check(glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3(kEps))), what);
    }

    void checkNear(const glm::mat3& a, const glm::mat3& b, const std::string& what)
    {
        bool equal = true;
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r)
                equal = equal && std::fabs(a[c][r] - b[c][r]) <= kEps;
        check(equal, what);
    }

    // Mirrors the memory layout of utils::GaussianDataSSBO without pulling in
    // the OpenGL-facing utils.hpp, so the tests exercise the real template.
    struct TestGaussian
    {
        glm::vec4 position;
        glm::vec4 color;
        glm::vec4 scale;
        glm::vec4 normal;
        glm::vec4 rotation; // packed (w, x, y, z), as everywhere else in the codebase
        glm::vec4 pbr;
    };

    glm::vec4 packQuat(const glm::quat& q) { return glm::vec4(q.w, q.x, q.y, q.z); }
    glm::quat unpackQuat(const glm::vec4& v) { return glm::quat(v.x, v.y, v.z, v.w); }

    // A handful of non-trivial orientations to sweep the quaternion paths.
    std::vector<glm::quat> sampleRotations()
    {
        std::vector<glm::quat> qs;
        qs.push_back(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));                                  // identity
        qs.push_back(glm::normalize(glm::quat(0.3f, 0.2f, -0.9f, 0.15f)));
        qs.push_back(glm::normalize(glm::quat(-0.5f, 0.5f, 0.5f, 0.5f)));
        qs.push_back(glm::angleAxis(glm::radians(37.0f),  glm::normalize(glm::vec3(1, 2, 3))));
        qs.push_back(glm::angleAxis(glm::radians(-120.0f), glm::normalize(glm::vec3(-4, 1, 0.5f))));
        qs.push_back(glm::angleAxis(glm::radians(180.0f), glm::vec3(0, 1, 0)));
        return qs;
    }

    void testDefaultIsIdentity()
    {
        std::printf("testDefaultIsIdentity\n");
        const auto cs = utils::CoordinateSystem::GltfYUp;

        checkNear(utils::gltfToCoordinateSystem(cs), glm::mat3(1.0f), "glTF target is the identity rotation");

        const glm::vec3 p(1.5f, -2.25f, 3.0f);
        checkNear(utils::convertPosition(p, cs), p, "positions are untouched");
        checkNear(utils::convertDirection(p, cs), p, "directions are untouched");

        for (const glm::quat& q : sampleRotations())
        {
            const glm::vec4 packed = packQuat(q);
            check(utils::convertRotationWxyz(packed, cs) == packed, "rotations are untouched");
        }

        // Guards the promise that existing pipelines see byte-identical output.
        TestGaussian g{};
        g.position = glm::vec4(1, 2, 3, 1);
        g.normal   = glm::vec4(0, 0, 1, 0);
        g.scale    = glm::vec4(0.1f, 0.2f, 1e-7f, 0);
        g.rotation = packQuat(glm::normalize(glm::quat(0.3f, 0.2f, -0.9f, 0.15f)));
        const TestGaussian before = g;

        utils::convertGaussianInPlace(g, cs);
        check(g.position == before.position && g.normal == before.normal &&
              g.scale == before.scale && g.rotation == before.rotation,
              "default export path is a no-op");
    }

    void testColmapAxisMapping()
    {
        std::printf("testColmapAxisMapping\n");
        const auto cs = utils::CoordinateSystem::ColmapYDown;

        checkNear(utils::convertPosition(glm::vec3(1, 0, 0), cs), glm::vec3(1, 0, 0),  "+X is preserved");
        checkNear(utils::convertPosition(glm::vec3(0, 1, 0), cs), glm::vec3(0, -1, 0), "+Y (up) becomes -Y (down)");
        checkNear(utils::convertPosition(glm::vec3(0, 0, 1), cs), glm::vec3(0, 0, -1), "+Z (towards viewer) becomes -Z");

        checkNear(utils::convertPosition(glm::vec3(2.5f, -3.5f, 4.0f), cs), glm::vec3(2.5f, 3.5f, -4.0f),
                  "general position maps as (x, -y, -z)");
    }

    void testFrameChangeIsAProperRotation()
    {
        std::printf("testFrameChangeIsAProperRotation\n");
        const glm::mat3 R = utils::gltfToCoordinateSystem(utils::CoordinateSystem::ColmapYDown);

        // det == +1 rather than -1: this must not be a mirror, or triangle
        // winding and normal orientation would silently invert.
        checkNear(glm::determinant(R), 1.0f, "determinant is +1 (rotation, not reflection)");
        checkNear(glm::transpose(R) * R, glm::mat3(1.0f), "matrix is orthonormal");

        // Handedness check stated directly in terms of the basis vectors.
        const glm::vec3 x = R * glm::vec3(1, 0, 0);
        const glm::vec3 y = R * glm::vec3(0, 1, 0);
        const glm::vec3 z = R * glm::vec3(0, 0, 1);
        checkNear(glm::cross(x, y), z, "converted basis stays right-handed");
    }

    void testConversionIsItsOwnInverse()
    {
        std::printf("testConversionIsItsOwnInverse\n");
        const auto cs = utils::CoordinateSystem::ColmapYDown;

        const glm::vec3 p(0.5f, -1.25f, 7.0f);
        checkNear(utils::convertPosition(utils::convertPosition(p, cs), cs), p, "position round-trips");

        for (const glm::quat& q : sampleRotations())
        {
            const glm::vec4 once  = utils::convertRotationWxyz(packQuat(q), cs);
            const glm::vec4 twice = utils::convertRotationWxyz(once, cs);

            // q and -q are the same rotation, so compare the rotation matrices.
            checkNear(glm::mat3_cast(unpackQuat(twice)), glm::mat3_cast(q), "rotation round-trips");
        }
    }

    void testQuaternionMatchesMatrix()
    {
        std::printf("testQuaternionMatchesMatrix\n");
        const auto cs = utils::CoordinateSystem::ColmapYDown;
        const glm::mat3 R = utils::gltfToCoordinateSystem(cs);

        // The load-bearing property: rotating the orientation quaternion has to
        // agree with rotating the orientation matrix. This is what catches a
        // (w,x,y,z) vs (x,y,z,w) packing mistake, which is otherwise invisible
        // until a splat renders scrambled.
        for (const glm::quat& q : sampleRotations())
        {
            const glm::quat converted = unpackQuat(utils::convertRotationWxyz(packQuat(q), cs));

            checkNear(glm::mat3_cast(converted), R * glm::mat3_cast(q), "quaternion path matches matrix path");
            checkNear(glm::length(converted), 1.0f, "converted quaternion stays unit length");
        }
    }

    void testCovarianceIsPreserved()
    {
        std::printf("testCovarianceIsPreserved\n");
        const auto cs = utils::CoordinateSystem::ColmapYDown;
        const glm::mat3 R = utils::gltfToCoordinateSystem(cs);

        // Scales are deliberately left alone. That is only correct if the whole
        // frame change is absorbed by the rotation term of the covariance, so
        // assert it on the covariance itself. The near-zero third scale is the
        // flat-surfel value the converter actually emits.
        const glm::vec3 scale(0.031f, 0.0075f, 1e-7f);

        for (const glm::quat& q : sampleRotations())
        {
            const glm::quat converted = unpackQuat(utils::convertRotationWxyz(packQuat(q), cs));

            const glm::mat3 S  = glm::mat3(glm::vec3(scale.x, 0, 0), glm::vec3(0, scale.y, 0), glm::vec3(0, 0, scale.z));
            const glm::mat3 M  = glm::mat3_cast(q) * S;
            const glm::mat3 M2 = glm::mat3_cast(converted) * S;

            checkNear(M2 * glm::transpose(M2), R * (M * glm::transpose(M)) * glm::transpose(R),
                      "covariance transforms as R * Sigma * R^T with scales unchanged");
        }
    }

    void testGaussianStructConversion()
    {
        std::printf("testGaussianStructConversion\n");
        const auto cs = utils::CoordinateSystem::ColmapYDown;

        TestGaussian g{};
        g.position = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
        g.normal   = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        g.color    = glm::vec4(0.25f, 0.5f, 0.75f, 0.9f);
        g.scale    = glm::vec4(0.031f, 0.0075f, 1e-7f, 0.0f);
        g.pbr      = glm::vec4(0.1f, 0.5f, 0.0f, 1.0f);
        g.rotation = packQuat(glm::normalize(glm::quat(0.3f, 0.2f, -0.9f, 0.15f)));

        const glm::vec4 originalScale = g.scale;
        const glm::vec4 originalColor = g.color;
        const glm::vec4 originalPbr   = g.pbr;

        utils::convertGaussianInPlace(g, cs);

        checkNear(glm::vec3(g.position), glm::vec3(1.0f, -2.0f, -3.0f), "position is rotated");
        checkNear(g.position.w, 1.0f, "homogeneous w is preserved");
        checkNear(glm::vec3(g.normal), glm::vec3(0.0f, -1.0f, 0.0f), "normal is rotated");
        checkNear(g.normal.w, 0.0f, "normal w is preserved");

        check(g.scale == originalScale, "scale is untouched");
        check(g.color == originalColor, "colour is untouched");
        check(g.pbr == originalPbr, "pbr is untouched");
    }

    void testContainerConversion()
    {
        std::printf("testContainerConversion\n");

        std::vector<TestGaussian> gaussians(3);
        for (int i = 0; i < 3; ++i)
        {
            gaussians[i].position = glm::vec4(float(i), 1.0f, 2.0f, 1.0f);
            gaussians[i].normal   = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
            gaussians[i].rotation = packQuat(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        }

        utils::convertGaussiansInPlace(gaussians, utils::CoordinateSystem::ColmapYDown);

        for (int i = 0; i < 3; ++i)
        {
            checkNear(glm::vec3(gaussians[i].position), glm::vec3(float(i), -1.0f, -2.0f), "every element is converted");
            checkNear(glm::vec3(gaussians[i].normal), glm::vec3(0.0f, 0.0f, -1.0f), "every normal is converted");
        }

        std::vector<TestGaussian> untouched(2);
        untouched[0].position = glm::vec4(4, 5, 6, 1);
        untouched[1].position = glm::vec4(7, 8, 9, 1);
        utils::convertGaussiansInPlace(untouched, utils::CoordinateSystem::GltfYUp);
        check(untouched[0].position == glm::vec4(4, 5, 6, 1) && untouched[1].position == glm::vec4(7, 8, 9, 1),
              "container conversion honours the default no-op");
    }

    void testLabels()
    {
        std::printf("testLabels\n");
        check(std::string(utils::toString(utils::CoordinateSystem::GltfYUp)).find("+Y up") != std::string::npos,
              "glTF label mentions +Y up");
        check(std::string(utils::toString(utils::CoordinateSystem::ColmapYDown)).find("+Y down") != std::string::npos,
              "COLMAP label mentions +Y down");

        // The enum values are persisted in the UI/CLI surface, so pin them.
        check(static_cast<unsigned int>(utils::CoordinateSystem::GltfYUp) == 0u, "GltfYUp stays 0");
        check(static_cast<unsigned int>(utils::CoordinateSystem::ColmapYDown) == 1u, "ColmapYDown stays 1");
    }
}

int main()
{
    std::printf("Mesh2Splat coordinate system tests\n\n");

    testDefaultIsIdentity();
    testColmapAxisMapping();
    testFrameChangeIsAProperRotation();
    testConversionIsItsOwnInverse();
    testQuaternionMatchesMatrix();
    testCovarianceIsPreserved();
    testGaussianStructConversion();
    testContainerConversion();
    testLabels();

    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
