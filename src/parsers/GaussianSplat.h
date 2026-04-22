//-----------------------------------------------------------------------------
// Copyright (c) Electronic Arts.  All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cassert>
#include <algorithm>

#define GLM_FORCE_XYZW_ONLY 1
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/quaternion.hpp>

struct BoundingBox
{
	glm::vec3 min;
	glm::vec3 max;

	// useful to grow a bounding box from many input boxes
	BoundingBox()
	{
		min = glm::vec3(std::numeric_limits<float>::max());
		max = -glm::vec3(std::numeric_limits<float>::max());
	}

	bool valid() const
	{
		return min != glm::vec3(std::numeric_limits<float>::max());
	}
	void grow(const glm::vec3 other)
	{
		min = glm::min(min, other);
		max = glm::max(max, other);
	}
	void grow(const BoundingBox& other)
	{
		min = glm::min(min, other.min);
		max = glm::max(max, other.max);
	}
	glm::vec3 fullSize() const
	{
		return max - min;
	}
	void getCorners(glm::vec4* const corners) const
	{
		corners[0] = glm::vec4(max.x, max.y, min.z, 1.f);
		corners[1] = glm::vec4(min.x, max.y, min.z, 1.f);
		corners[2] = glm::vec4(min.x, min.y, min.z, 1.f);
		corners[3] = glm::vec4(max.x, min.y, min.z, 1.f);
		corners[4] = glm::vec4(max.x, max.y, max.z, 1.f);
		corners[5] = glm::vec4(min.x, max.y, max.z, 1.f);
		corners[6] = glm::vec4(min.x, min.y, max.z, 1.f);
		corners[7] = glm::vec4(max.x, min.y, max.z, 1.f);
	}
	BoundingBox getTransformedAabb(const glm::mat4& T) const
	{
		glm::vec4 corners[8];
		getCorners(corners);

		BoundingBox result;
		for (uint32_t i = 0; i < 8; ++i)
		{
			auto c = glm::vec3(T * corners[i]);
			result.min = glm::min(result.min, c);
			result.max = glm::max(result.max, c);
		}

		return result;
	}
};

enum class GaussianSplattingMode : uint32_t
{
	Unknown = 0,
	// see SplatElementGeometry
	GeometryOnly,
	// large, all data from 3DGS with padding for simpler access, see SplatElement
	Classic,
	//
	PBR
};

// Scale packing/unpacking (log/exp)
inline glm::vec3 packScale(glm::vec3 size)
{
	return glm::log(size);
}
inline glm::vec3 unpackScale(glm::vec3 size)
{
	return glm::exp(size);
}

// @return 0..1, roughly around x=-4 it's 0.0f, around x=+4 it's 1.0f
inline float sigmoid(float x)
{
	// CUDA Gaussian Splatting implementation
	// https://github.com/graphdeco-inria/diff-gaussian-rasterization/blob/8064f52ca233942bdec2d1a1451c026deedd320b/cuda_rasterizer/auxiliary.h
	return 1.0f / (1.0f + expf(-x));
}

// Inverse sigmoid (unsigmoid)
inline float unsigmoid(float x)
{
	return -logf(1.0f / x - 1.0f);
}

// Inverse sigmoid with clamping for numerical stability
inline float inverseSigmoid(float a)
{
	const float eps = 1e-6f;
	if (!std::isfinite(a)) a = 0.5f;
	a = std::clamp(a, eps, 1.0f - eps);
	return std::log(a / (1.0f - a));
}

// Matrix from quaternion (CUDA 3DGS compatible)
inline glm::mat3 matrixFromQuaternion(glm::vec4 q)
{
	// has positive effect on some
	q = glm::normalize(q);

	// CUDA Gaussian Splatting implementation
	// https://github.com/graphdeco-inria/diff-gaussian-rasterization/blob/8064f52ca233942bdec2d1a1451c026deedd320b/cuda_rasterizer/forward.cu

	float r = q.x;
	float x = q.y;
	float y = q.z;
	float z = q.w;

	// Compute rotation matrix from quaternion
	glm::mat3 m(
		1.f - 2.f * (y * y + z * z), 2.f * (x * y - r * z), 2.f * (x * z + r * y),
		2.f * (x * y + r * z), 1.f - 2.f * (x * x + z * z), 2.f * (y * z - r * x),
		2.f * (x * z - r * y), 2.f * (y * z + r * x), 1.f - 2.f * (x * x + y * y));

	return m;
}

// GaussianSplattingMode::GeometryOnly
#pragma pack(push, 1)
struct SplatElementGeometry
{
	// xyz:pos
	glm::vec3 pos;
	//
	uint32_t dummy1;
	// rotation as quaternion, use setQuat() / getQuat() to access
	glm::vec4 rot;
	// .xyz:scale
	glm::vec3 linearScale;
	// approximation for SH, will not work for PBR properties
	glm::u8vec4 colorForLOD;

	// reflection, for loadProperties()
	static const char* getProperties()
	{
		return "x,y,z,,"
			   "rot_0,rot_1,rot_2,rot_3,"
			   "scale_0,scale_1,scale_2,,";
	}

	// compute AABB of a splat
	// @param gaussianScale GAUSSIAN_SCALE, smaller is better for performance, large can be better for quality
	BoundingBox computeMinMax(const float gaussianScale) const
	{
		assert(linearScale.x >= 0.0f);
		assert(linearScale.y >= 0.0f);
		assert(linearScale.z >= 0.0f);

		glm::mat4 mat = glm::translate(glm::mat4(1.0f), pos) * glm::mat4(glm::inverse(matrixFromQuaternion(rot)));

		BoundingBox localBox;
		localBox.min = -glm::vec3(linearScale * gaussianScale);
		localBox.max = glm::vec3(linearScale * gaussianScale);

		BoundingBox globalBox = localBox.getTransformedAabb(mat);

		assert(globalBox.valid());
		return globalBox;
	}
};
#pragma pack(pop)

// GaussianSplattingMode::classic
// later we should use multiple streams and make this struct use what SplatElementGeometry doesn't have
// needs to be in sync with HLSL class of the same name
// also HLSL function: splatElement.fetch() and SplatContext::stride
#pragma pack(push, 1)
struct SplatElement
{
	// xyz:pos, w:non linear opacity
	glm::vec4 pos;
	// rotation as quaternion, use setQuat() / getQuat() to access
	glm::vec4 rot;
	// .xyz:scale, use packScale(), unpackScale()
	glm::vec3 linearScale;
	// unused, padding
	float scaleW;
	// x:red, y:green, z:blue todo: inefficient, we waste W
	glm::vec4 SH[16];

	glm::vec3 getColor() const { return glm::vec3(SH[0].x, SH[0].y, SH[0].z) * 0.2820948f + 0.5f; }
	void setColor(glm::vec3 color)
	{
		glm::vec3 col = (color - glm::vec3(0.5f)) / 0.2820948f;
		SH[0]		  = glm::vec4(col.x, col.y, col.z, 0.0f);
	}

	glm::quat getQuat() const
	{
		// avoid quat ctor: https://twitter.com/MittringMartin/status/1756041311952806261
		glm::quat ret;

		ret.x = rot.y;
		ret.y = rot.z;
		ret.z = rot.w;
		ret.w = rot.x;

		return ret;
	}
	void setQuat(const glm::quat n) { rot = glm::vec4(n.w, n.x, n.y, n.z); }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MergedGaussian
{
	glm::vec3 position;
	glm::quat rotation;
	// 0..1
	float linearOpacity;
	glm::vec3 linearScale;
	glm::vec3 color;
	glm::vec2 metallicRoughness;
	glm::vec3 normal;
};
#pragma pack(pop)

MergedGaussian mergeChildGaussians(const std::vector<MergedGaussian>& childGaussians, float mergingConstant);
