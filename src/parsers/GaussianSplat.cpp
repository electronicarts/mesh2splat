//-----------------------------------------------------------------------------
// Copyright (c) Electronic Arts.  All rights reserved.
//-----------------------------------------------------------------------------

#include "GaussianSplat.h"
#include <cmath>

// Quaternion multiplication helper (a * b)
glm::quat qmul(glm::quat a, glm::vec4 b)
{
	glm::quat q;

	q.x = a.x * b.w + a.w * b.x + a.y * b.z - a.z * b.y;
	q.y = a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z;
	q.z = a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x;
	q.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;

	return q;
}

// Diagonalize a symmetric 3x3 matrix using Jacobi rotations
// Returns quaternion representing rotation to principal axes; outDiagonal receives eigenvalues
glm::quat Diagonalizer(const glm::mat3& A, glm::vec3& outDiagonal)
{
	assert(A[0][1] == A[1][0]);
	assert(A[0][2] == A[2][0]);
	assert(A[1][2] == A[2][1]);
	// A must be a symmetric matrix.
	// returns orientation of the principle axes.
	// returns quaternion q such that its corresponding column major matrix Q
	// can be used to Diagonalize A
	// Diagonal matrix D = transpose(Q) * A * (Q);  thus  A == Q*D*QT
	// The directions of q (cols of Q) are the eigenvectors D's diagonal is the eigenvalues
	// As per 'col' convention if float3x3 Q = qgetmatrix(q); then Q*v = q*v*conj(q)
	int maxsteps = 24; // certainly wont need that many.

	glm::quat q;
	// avoid quat CTOR to avoid problems with GLM defines
	// see https://twitter.com/MittringMartin/status/1756041311952806261
	q.x = q.y = q.z = 0.0f;
	q.w				= 1.0f;

	for (int i = 0; i < maxsteps; i++)
	{
		glm::mat3 Q = glm::mat3_cast(q); // Q*v == q*v*conj(q)
		glm::mat3 D = transpose(Q) * A * Q; // A = Q*D*Q^T
		glm::vec3 offdiag(D[1][2], D[0][2], D[0][1]); // elements not on the diagonal
		glm::vec3 om(fabsf(offdiag.x), fabsf(offdiag.y), fabsf(offdiag.z)); // mag of each offdiag elem

		outDiagonal = glm::vec3(D[0][0], D[1][1], D[2][2]);

		int k  = (om.x > om.y && om.x > om.z) ? 0 : (om.y > om.z) ? 1
																  : 2; // index of largest element of offdiag
		int k1 = (k + 1) % 3;
		int k2 = (k + 2) % 3;
		if (offdiag[k] == 0.0f)
			break; // diagonal already
		float thet = (D[k2][k2] - D[k1][k1]) / (2.0f * offdiag[k]);
		float sgn  = (thet > 0.0f) ? 1.0f : -1.0f;
		thet *= sgn; // make it positive
		float t = sgn / (thet + ((thet < 1.E6f) ? sqrtf(thet * thet + 1.0f) : thet)); // sign(T)/(|T|+sqrt(T^2+1))
		float c = 1.0f / sqrtf(t * t + 1.0f); //  c= 1/(t^2+1) , t=s/c
		if (c == 1.0f)
			break; // no room for improvement - reached machine precision.
		glm::vec4 jr(0, 0, 0, 0); // jacobi rotation for this iteration.
		jr[k] = sgn * sqrtf((1.0f - c) / 2.0f); // using 1/2 angle identity sin(a/2) = sqrt((1-cos(a))/2)
		jr[k] *= -1.0f; // note we want a final result semantic that takes D to A, not A to D
		jr.w = sqrtf(1.0f - (jr[k] * jr[k]));
		if (jr.w == 1.0f)
			break; // reached limits of floating point precision
		q = qmul(q, jr);
		q = normalize(q);
	}
	return q;
}

float ellipseSurface(glm::vec3 scale)
{
	return scale[0] * scale[1] + scale[0] * scale[2] + scale[1] * scale[2];
}

float logistic(float x)
{
	return 1 / (1 + exp(-(x - 5)));
}

float downscaler(float x)
{
	float v = logistic(x);
	return v / (v + expf(-10.0f * (x - 0.5f * v)));
}

// Function to merge Gaussians with covariance computation
//Reading from https://github.com/graphdeco-inria/gaussian-hierarchy/blob/677c8553dc64dfd62c272eca94a291a277733113/ClusterMerger.cpp
MergedGaussian mergeGaussians(const std::vector<MergedGaussian>& gaussiansToMerge, float mergingConstant)
{
	float totalCoverageWeight = 0.0f; // sum of (splat.opacity * volume)

	MergedGaussian representative;
	representative.color	   = { 0.0f, 0.0f, 0.0f };
	representative.position	   = { 0.0f, 0.0f, 0.0f };
	representative.linearScale = { 0.0f, 0.0f, 0.0f };
	representative.rotation	   = { 0.0f, 0.0f, 0.0f, 0.0f };
	representative.linearOpacity	   = 0.0f;
	representative.metallicRoughness = { 0.0, 0.0 };
	representative.normal			 = { 0.0, 0.0, 0.0 };

	// We'll need coveragePoints and coverageWeights for PCA:
	std::vector<glm::vec3> coveragePoints;
	std::vector<float> coverageWeights;
	coveragePoints.reserve(gaussiansToMerge.size() * 7);
	coverageWeights.reserve(gaussiansToMerge.size() * 7);
	float sumW			= 0;
	glm::vec3 meanColor = { 0, 0, 0 };
	for (auto& splat : gaussiansToMerge)
	{
		glm::mat3 m	 = glm::toMat3(splat.rotation);
		glm::vec3 e1 = glm::normalize(m[0]) * splat.linearScale.x * mergingConstant;
		glm::vec3 e2 = glm::normalize(m[1]) * splat.linearScale.y * mergingConstant;
		glm::vec3 e3 = glm::normalize(m[2]) * splat.linearScale.z * mergingConstant;

		float opacity = splat.linearOpacity;
		float surface = ellipseSurface(glm::vec3(glm::length(e1), glm::length(e2), glm::length(e3)));

		assert(opacity > 0);

		float w = opacity * surface;

		totalCoverageWeight += w;
		meanColor += splat.color;
		representative.color += w * splat.color;
		representative.normal += w * splat.normal;
		representative.metallicRoughness += w * splat.metallicRoughness;

		coveragePoints.push_back(splat.position);
		coveragePoints.push_back(splat.position + e1);
		coveragePoints.push_back(splat.position - e1);
		coveragePoints.push_back(splat.position + e2);
		coveragePoints.push_back(splat.position - e2);
		coveragePoints.push_back(splat.position + e3);
		coveragePoints.push_back(splat.position - e3);

		for (int k = 0; k < 7; ++k)
		{
			coverageWeights.push_back(w);
			assert(!std::isnan(w));
			assert(w > 0);
			sumW += w;
		}
	}

	assert(sumW > 0);
	for (auto& w : coverageWeights)
	{
		w /= sumW;
	}

	representative.color /= totalCoverageWeight;
	glm::vec3 avgColor = representative.color;
	float alpha		   = sqrtf(1.0f / gaussiansToMerge.size());
	meanColor /= static_cast<float>(gaussiansToMerge.size());
	glm::vec3 finalColor = meanColor + alpha * (avgColor - meanColor);

	representative.color = finalColor;
	representative.metallicRoughness /= totalCoverageWeight;
	representative.normal /= totalCoverageWeight;

	glm::vec3 weightedMean(0.0f);
	for (size_t i = 0; i < coveragePoints.size(); ++i)
	{
		assert(!std::isnan(coverageWeights[i]));

		weightedMean += coveragePoints[i] * coverageWeights[i];
	}

	representative.position = { weightedMean.x, weightedMean.y, weightedMean.z };

	const int numPoints = int(coveragePoints.size());

	std::vector<glm::vec3> VarMat(numPoints);

	for (int i = 0; i < numPoints; ++i)
	{
		VarMat[i] = coveragePoints[i] - weightedMean;
	}

	std::vector<float> Wdiag(coveragePoints.size());

	for (size_t i = 0; i < coveragePoints.size(); i++)
	{
		Wdiag[i] = coverageWeights[i];
	}

	glm::mat3 cov(0.0f);

	for (int i = 0; i < numPoints; ++i)
	{
		float weight  = coverageWeights[i];
		glm::vec3 var = VarMat[i];

		assert(!std::isnan(weight));
		for (int row = 0; row < 3; ++row)
		{
			assert(!std::isnan(var[row]));
			for (int col = 0; col < 3; ++col)
			{
				assert(!std::isnan(var[col]));
				cov[row][col] += var[row] * var[col] * weight;
			}
		}
	}
	assert(!std::isnan(cov[0][0]));

	glm::vec3 eval;
	glm::quat q = Diagonalizer(cov, eval);
	glm::mat3 Q = glm::toMat3(q);

	auto v1	  = Q[0];
	auto v2	  = Q[1];
	auto v3	  = Q[2];
	auto test = glm::cross(v1, v2);

	if (glm::dot(test, v3) < 0)
		Q[2] *= -1;

	representative.rotation = glm::normalize(q);

	representative.linearScale = glm::vec3(
		std::sqrt(std::max(std::abs(eval.x), 1e-8f)),
		std::sqrt(std::max(std::abs(eval.y), 1e-8f)),
		std::sqrt(std::max(std::abs(eval.z), 1e-8f)));

	glm::mat3 m	  = Q;
	glm::vec3 e_1 = glm::normalize(m[0]) * representative.linearScale.x * mergingConstant;
	glm::vec3 e_2 = glm::normalize(m[1]) * representative.linearScale.y * mergingConstant;
	glm::vec3 e_3 = glm::normalize(m[2]) * representative.linearScale.z * mergingConstant;

	representative.linearOpacity = totalCoverageWeight / ellipseSurface(glm::vec3(glm::length(e_1), glm::length(e_2), glm::length(e_3)));
	assert(representative.linearOpacity > 0);

	return representative;
}

MergedGaussian mergeChildGaussians(const std::vector<MergedGaussian>& childGaussians, float mergingConstant)
{
	if (childGaussians.empty())
	{
		// Return a default/invalid gaussian
		MergedGaussian empty{};
		return empty;
	}
	if (childGaussians.size() > 1)
	{
		return mergeGaussians(childGaussians, mergingConstant);
	}
	return childGaussians[0];
}
