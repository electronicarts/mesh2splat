#include "normalizedUvUnwrapping.hpp"

static void RandomColor(uint8_t* color)
{
	for (int i = 0; i < 3; i++)
		color[i] = uint8_t((rand() % 255 + 192) * 0.5f);
}

static void SetPixel(uint8_t* dest, int destWidth, int x, int y, const uint8_t* color)
{
	uint8_t* pixel = &dest[x * 3 + y * (destWidth * 3)];
	pixel[0] = color[0];
	pixel[1] = color[1];
	pixel[2] = color[2];
}

// https://github.com/miloyip/line/blob/master/line_bresenham.c
// License: public domain.
static void RasterizeLine(uint8_t* dest, int destWidth, const int* p1, const int* p2, const uint8_t* color)
{
	const int dx = abs(p2[0] - p1[0]), sx = p1[0] < p2[0] ? 1 : -1;
	const int dy = abs(p2[1] - p1[1]), sy = p1[1] < p2[1] ? 1 : -1;
	int err = (dx > dy ? dx : -dy) / 2;
	int current[2];
	current[0] = p1[0];
	current[1] = p1[1];
	while (SetPixel(dest, destWidth, current[0], current[1], color), current[0] != p2[0] || current[1] != p2[1])
	{
		const int e2 = err;
		if (e2 > -dx) { err -= dy; current[0] += sx; }
		if (e2 < dy) { err += dx; current[1] += sy; }
	}
}

/*
https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
Copyright Dmitry V. Sokolov

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
static void RasterizeTriangle(uint8_t* dest, int destWidth, const int* t0, const int* t1, const int* t2, const uint8_t* color)
{
	if (t0[1] > t1[1]) std::swap(t0, t1);
	if (t0[1] > t2[1]) std::swap(t0, t2);
	if (t1[1] > t2[1]) std::swap(t1, t2);
	int total_height = t2[1] - t0[1];
	for (int i = 0; i < total_height; i++) {
		bool second_half = i > t1[1] - t0[1] || t1[1] == t0[1];
		int segment_height = second_half ? t2[1] - t1[1] : t1[1] - t0[1];
		float alpha = (float)i / total_height;
		float beta = (float)(i - (second_half ? t1[1] - t0[1] : 0)) / segment_height;
		int A[2], B[2];
		for (int j = 0; j < 2; j++) {
			A[j] = int(t0[j] + (t2[j] - t0[j]) * alpha);
			B[j] = int(second_half ? t1[j] + (t2[j] - t1[j]) * beta : t0[j] + (t1[j] - t0[j]) * beta);
		}
		if (A[0] > B[0]) std::swap(A, B);
		for (int j = A[0]; j <= B[0]; j++)
			SetPixel(dest, destWidth, j, t0[1] + i, color);
	}
}

#if !OBJ_TRIANGULATE
//  public-domain code by Darel Rex Finley, 2007
// http://alienryderflex.com/polygon_fill/
static void RasterizePolygon(uint8_t* dest, int destWidth, int vertices[][2], const int vertexCount, const uint8_t* color)
{
	int IMAGE_TOP = INT_MAX, IMAGE_BOT = 0, IMAGE_LEFT = INT_MAX, IMAGE_RIGHT = 0;
	for (int i = 0; i < vertexCount; i++) {
		const int* vertex = vertices[i];
		IMAGE_TOP = vertex[1] < IMAGE_TOP ? vertex[1] : IMAGE_TOP;
		IMAGE_BOT = vertex[1] > IMAGE_BOT ? vertex[1] : IMAGE_BOT;
		IMAGE_LEFT = vertex[0] < IMAGE_LEFT ? vertex[0] : IMAGE_LEFT;
		IMAGE_RIGHT = vertex[0] > IMAGE_RIGHT ? vertex[0] : IMAGE_RIGHT;
	}
	int  nodes, nodeX[255], pixelX, pixelY, i, j, swap;
	//  Loop through the rows of the image.
	for (pixelY = IMAGE_TOP; pixelY < IMAGE_BOT; pixelY++) {
		//  Build a list of nodes.
		nodes = 0; j = vertexCount - 1;
		for (i = 0; i < vertexCount; i++) {
			if (vertices[i][1] < (double)pixelY && vertices[j][1] >= (double)pixelY || vertices[j][1] < (double)pixelY && vertices[i][1] >= (double)pixelY) {
				nodeX[nodes++] = (int)(vertices[i][0] + (pixelY - vertices[i][1]) / (vertices[j][1] - vertices[i][1]) * (vertices[j][0] - vertices[i][0]));
			}
			j = i;
		}
		//  Sort the nodes, via a simple “Bubble” sort.
		i = 0;
		while (i < nodes - 1) {
			if (nodeX[i] > nodeX[i + 1]) {
				swap = nodeX[i]; nodeX[i] = nodeX[i + 1]; nodeX[i + 1] = swap; if (i) i--;
			}
			else {
				i++;
			}
		}
		//  Fill the pixels between node pairs.
		for (i = 0; i < nodes; i += 2) {
			if (nodeX[i] >= IMAGE_RIGHT)
				break;
			if (nodeX[i + 1] > IMAGE_LEFT) {
				if (nodeX[i] < IMAGE_LEFT)
					nodeX[i] = IMAGE_LEFT;
				if (nodeX[i + 1] > IMAGE_RIGHT)
					nodeX[i + 1] = IMAGE_RIGHT;
				for (pixelX = nodeX[i]; pixelX < nodeX[i + 1]; pixelX++)
					SetPixel(dest, destWidth, pixelX, pixelY, color);
			}
		}
	}
}
#endif

void generateNormalizedUvCoordinatesPerFace(int& uvSpaceWidth, int& uvSpaceHeight, std::vector<Mesh>& meshes)
{
	for (auto& mesh : meshes)
	{
		generateNormalizedUvCoordinatesPerFace(uvSpaceWidth, uvSpaceHeight, mesh);
	}
}

void generateNormalizedUvCoordinatesPerFace(int& uvSpaceWidth, int& uvSpaceHeight, Mesh& inputMesh)
{
	xatlas::MeshDecl meshDecl;
	std::vector<float> vertexPositions;
	std::vector<float> vertexUVs;
	std::vector<float> vertexNormals;

	for (auto& face : inputMesh.faces)
	{
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				vertexPositions.push_back(face.pos[i][j]);
				vertexNormals.push_back(face.normal[i][j]);
				if(j < 2) vertexUVs.push_back(face.uv[i][j]);
			}
			
		}
	}

	meshDecl.vertexCount = (uint32_t) vertexPositions.size() / 3;
	meshDecl.vertexPositionData = vertexPositions.data();
	meshDecl.vertexPositionStride = sizeof(float) * 3;

	if (!vertexUVs.empty())
	{
		meshDecl.vertexUvData = vertexUVs.data();
		meshDecl.vertexUvStride = sizeof(float) * 2;
	}

	if (!vertexNormals.empty())
	{
		meshDecl.vertexNormalData = vertexNormals.data();
		meshDecl.vertexNormalStride = sizeof(float) * 3;
	}
	xatlas::Atlas* atlas = xatlas::Create();
	xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl, 1);

	if (error != xatlas::AddMeshError::Success) {
		xatlas::Destroy(atlas);
		printf("\rError adding mesh '%s': %s\n", inputMesh.name.c_str(), xatlas::StringForEnum(error));
		exit(1);
	}

	xatlas::Generate(atlas);
	uvSpaceWidth = atlas->width;
	uvSpaceHeight = atlas->height;

	for (uint32_t i = 0; i < atlas->meshCount; i++)
	{
		const xatlas::Mesh& mesh = atlas->meshes[i];
		//std::cout << "Atlas Vertex count: " << mesh.vertexCount << "\n" << std::endl;
		//std::cout << "Original mesh Vertex count: " << inputMesh.faces.size() * 3 << "\n" << std::endl;

		for (uint32_t v = 0; v < mesh.vertexCount; v++) {
			const xatlas::Vertex& vertex = mesh.vertexArray[v];

			// Assuming the mesh has triangle faces
			int faceIndex = mesh.vertexArray[v].xref / 3;
			int vertexIndex = mesh.vertexArray[v].xref % 3;

			if (faceIndex < inputMesh.faces.size()) {
				inputMesh.faces[faceIndex].normalizedUvs[vertexIndex] = glm::vec2(vertex.uv[0] / (float)uvSpaceWidth, vertex.uv[1] / (float)uvSpaceHeight);
			}
			else {
				std::cerr << "Warning: faceIndex " << faceIndex << " out of bounds." << std::endl;
			}

		}
	}

#if DRAW_UV_MAPPING 
	if (atlas->width > 0 && atlas->height > 0) {
		printf("Rasterizing result...\n");
		// Dump images.
		std::vector<uint8_t> outputTrisImage, outputChartsImage;
		const uint32_t imageDataSize = atlas->width * atlas->height * 3;
		outputTrisImage.resize(atlas->atlasCount * imageDataSize);
		outputChartsImage.resize(atlas->atlasCount * imageDataSize);
		for (uint32_t i = 0; i < atlas->meshCount; i++) {
			const xatlas::Mesh& mesh = atlas->meshes[i];
			// Rasterize mesh triangles.
			const uint8_t white[] = { 255, 255, 255 };
#if OBJ_TRIANGULATE
			const uint32_t faceCount = mesh.indexCount / 3;
#else
			auto faceCount = (const uint32_t)inputMesh.faces.size();
#endif
			uint32_t faceFirstIndex = 0;
			for (uint32_t f = 0; f < faceCount; f++) {
				int32_t atlasIndex = -1;
				int verts[255][2];
#if OBJ_TRIANGULATE
				const uint32_t faceVertexCount = 3;
#else
				const uint32_t faceVertexCount = 3;
#endif
				for (uint32_t j = 0; j < faceVertexCount; j++) {
					const xatlas::Vertex& v = mesh.vertexArray[mesh.indexArray[faceFirstIndex + j]];
					atlasIndex = v.atlasIndex; // The same for every vertex in the face.
					verts[j][0] = int(v.uv[0]);
					verts[j][1] = int(v.uv[1]);
				}
				if (atlasIndex < 0)
					continue; // Skip faces that weren't atlased.
				uint8_t color[3];
				RandomColor(color);
				uint8_t* imageData = &outputTrisImage[atlasIndex * imageDataSize];
#if OBJ_TRIANGULATE
				RasterizeTriangle(imageData, atlas->width, verts[0], verts[1], verts[2], color);
#else
				if (faceVertexCount == 3)
					RasterizeTriangle(imageData, atlas->width, verts[0], verts[1], verts[2], color);
				else
					RasterizePolygon(imageData, atlas->width, verts, (int)faceVertexCount, color);
#endif
				for (uint32_t j = 0; j < faceVertexCount; j++)
					RasterizeLine(imageData, atlas->width, verts[j], verts[(j + 1) % faceVertexCount], white);
				faceFirstIndex += faceVertexCount;
			}
			// Rasterize mesh charts.
			for (uint32_t j = 0; j < mesh.chartCount; j++) {
				const xatlas::Chart* chart = &mesh.chartArray[j];
				uint8_t color[3];
				RandomColor(color);
				for (uint32_t k = 0; k < chart->faceCount; k++) {
					const uint32_t face = chart->faceArray[k];
#if OBJ_TRIANGULATE
					const uint32_t faceVertexCount = 3;
					faceFirstIndex = face * 3;
#else
					const uint32_t faceVertexCount = 3;
					faceFirstIndex = 0;
					for (uint32_t l = 0; l < face; l++)
						faceFirstIndex += 3;
#endif
					int verts[255][2];
					for (uint32_t l = 0; l < faceVertexCount; l++) {
						const xatlas::Vertex& v = mesh.vertexArray[mesh.indexArray[faceFirstIndex + l]];
						verts[l][0] = int(v.uv[0]);
						verts[l][1] = int(v.uv[1]);
					}
					uint8_t* imageData = &outputChartsImage[chart->atlasIndex * imageDataSize];
#if OBJ_TRIANGULATE
					RasterizeTriangle(imageData, atlas->width, verts[0], verts[1], verts[2], color);
#else
					if (faceVertexCount == 3)
						RasterizeTriangle(imageData, atlas->width, verts[0], verts[1], verts[2], color);
					else
						RasterizePolygon(imageData, atlas->width, verts, (int)faceVertexCount, color);
#endif
					for (uint32_t l = 0; l < faceVertexCount; l++)
						RasterizeLine(imageData, atlas->width, verts[l], verts[(l + 1) % faceVertexCount], white);
				}
			}
		}
		for (uint32_t i = 0; i < atlas->atlasCount; i++) {
			char filename[256];
			snprintf(filename, sizeof(filename), "example_tris%02u.tga", i);
			printf("Writing '%s'...\n", filename);
			stbi_write_tga(filename, atlas->width, atlas->height, 3, &outputTrisImage[i * imageDataSize]);
			snprintf(filename, sizeof(filename), "example_charts%02u.tga", i);
			printf("Writing '%s'...\n", filename);
			stbi_write_tga(filename, atlas->width, atlas->height, 3, &outputChartsImage[i * imageDataSize]);
		}
	}


#endif
}