/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BufferWrapper;

// -----------------------------------------------------------------------
// M3 geoset
// -----------------------------------------------------------------------

struct M3Geoset {
	uint32_t unknown0 = 0;
	uint32_t nameCharStart = 0;
	uint32_t nameCharCount = 0;
	uint32_t indexStart = 0;
	uint32_t indexCount = 0;
	uint32_t vertexStart = 0;
	uint32_t vertexCount = 0;
	uint32_t unknown1 = 0;
	uint32_t unknown2 = 0;
};

// -----------------------------------------------------------------------
// M3 LOD level
// -----------------------------------------------------------------------

struct M3LODLevel {
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
};

// -----------------------------------------------------------------------
// M3 render batch
// -----------------------------------------------------------------------

struct M3RenderBatch {
	uint16_t unknown0 = 0;
	uint16_t unknown1 = 0;
	uint16_t geosetIndex = 0;
	uint16_t materialIndex = 0;
};

// -----------------------------------------------------------------------
// M3 Loader class
// -----------------------------------------------------------------------

class M3Loader {
public:
	/**
	 * Construct a new M3Loader instance.
	 * @param data
	 */
	explicit M3Loader(BufferWrapper& data);

	/**
	 * Convert a chunk ID to a string.
	 * @param chunkID
	 */
	static std::string fourCCToString(uint32_t chunkID);

	/**
	 * Load the M3 model.
	 */
	void load();

	bool isLoaded = false;

	// Vertex data
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<float> uv;
	std::vector<float> uv1;
	std::vector<float> uv2;
	std::vector<float> uv3;
	std::vector<float> uv4;
	std::vector<float> uv5;
	std::vector<float> tangents;
	std::vector<uint16_t> indices;

	// String block
	BufferWrapper* stringBlock = nullptr;

	// Geosets
	std::vector<M3Geoset> geosets;

	// LOD data
	uint32_t lodCount = 0;
	uint32_t geosetCountPerLOD = 0;
	std::vector<M3LODLevel> lodLevels;

	// Render batches
	std::vector<M3RenderBatch> renderBatches;

private:
	BufferWrapper& data;

	// Owned string block data
	std::unique_ptr<BufferWrapper> ownedStringBlock;

	void parseChunk_M3DT(uint32_t chunkSize);
	void parseChunk_MES3(uint32_t chunkSize);
	void parseChunk_M3SI(uint32_t chunkSize);
	void parseChunk_M3CL(uint32_t chunkSize);

	void parseBufferChunk(uint32_t chunkSize, uint32_t chunkID, uint32_t propertyA, uint32_t propertyB);
	std::vector<float> ReadBufferAsFormat(const std::string& format, uint32_t chunkSize);
	std::vector<uint16_t> ReadBufferAsFormatU16(const std::string& format, uint32_t chunkSize);

	void parseSubChunk_VSTR(uint32_t chunkSize);
	void parseSubChunk_VGEO(uint32_t numGeosets);
	void parseSubChunk_LODS(uint32_t numLODs, uint32_t numGeosetsPerLOD);
	void parseSubChunk_RBAT(uint32_t numBatches);
};
