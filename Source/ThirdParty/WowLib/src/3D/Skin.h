/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

struct SubMesh {
	uint16_t submeshID;
	uint16_t level;
	uint16_t vertexStart;
	uint16_t vertexCount;
	uint32_t triangleStart;
	uint16_t triangleCount;
	uint16_t boneCount;
	uint16_t boneStart;
	uint16_t boneInfluences;
	uint16_t centerBoneIndex;
	std::array<float, 3> centerPosition;
	std::array<float, 3> sortCenterPosition;
	float sortRadius;
};

struct TextureUnit {
	uint8_t flags;
	int8_t priority;
	uint16_t shaderID;
	uint16_t skinSectionIndex;
	uint16_t flags2;
	uint16_t colorIndex;
	uint16_t materialIndex;
	uint16_t materialLayer;
	uint16_t textureCount;
	uint16_t textureComboIndex;
	uint16_t textureCoordComboIndex;
	uint16_t textureWeightComboIndex;
	uint16_t textureTransformComboIndex;
};

class Skin {
public:
	Skin(uint32_t fileDataID);

	void load();

	uint32_t fileDataID;
	std::string fileName;
	bool isLoaded;

	uint32_t bones = 0;
	std::vector<uint16_t> indices;
	std::vector<uint16_t> triangles;
	std::vector<uint8_t> properties;
	std::vector<SubMesh> subMeshes;
	std::vector<TextureUnit> textureUnits;
};
