/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class BufferWrapper;

// -----------------------------------------------------------------------
// WMO sub-structures
// -----------------------------------------------------------------------

struct WMOFogEntry {
	float end = 0.0f;
	float startScalar = 0.0f;
	uint32_t color = 0;
};

struct WMOFog {
	uint32_t flags = 0;
	std::vector<float> position;
	float radiusSmall = 0.0f;
	float radiusLarge = 0.0f;
	WMOFogEntry fog;
	WMOFogEntry underwaterFog;
};

struct WMOMaterial {
	uint32_t flags = 0;
	uint32_t shader = 0;
	uint32_t blendMode = 0;
	uint32_t texture1 = 0;
	uint32_t color1 = 0;
	uint32_t color1b = 0;
	uint32_t texture2 = 0;
	uint32_t color2 = 0;
	uint32_t groupType = 0;
	uint32_t texture3 = 0;
	uint32_t color3 = 0;
	uint32_t flags3 = 0;
	std::vector<uint32_t> runtimeData;
};

struct WMOPortalInfo {
	uint16_t startVertex = 0;
	uint16_t count = 0;
	std::vector<float> plane;
};

struct WMOPortalRef {
	uint16_t portalIndex = 0;
	uint16_t groupIndex = 0;
	int16_t side = 0;
};

struct WMOGroupInfo {
	uint32_t flags = 0;
	std::vector<float> boundingBox1;
	std::vector<float> boundingBox2;
	int32_t nameIndex = 0;
};

struct WMODoodadSet {
	std::string name;
	uint32_t firstInstanceIndex = 0;
	uint32_t doodadCount = 0;
	uint32_t unused = 0;
};

struct WMODoodad {
	uint32_t offset = 0;
	uint8_t flags = 0;
	std::vector<float> position;
	std::vector<float> rotation;
	float scale = 0.0f;
	std::vector<uint8_t> color;
};

struct WMOLiquidVertex {
	uint32_t data = 0;
	float height = 0.0f;
};

struct WMOLiquid {
	uint32_t vertX = 0;
	uint32_t vertY = 0;
	uint32_t tileX = 0;
	uint32_t tileY = 0;
	std::vector<WMOLiquidVertex> vertices;
	std::vector<uint8_t> tiles;
	std::vector<float> corner;
	uint16_t materialID = 0;
};

struct WMORenderBatch {
	std::vector<uint16_t> possibleBox1;
	std::vector<uint16_t> possibleBox2;
	uint32_t firstFace = 0;
	uint16_t numFaces = 0;
	uint16_t firstVertex = 0;
	uint16_t lastVertex = 0;
	uint8_t flags = 0;
	uint8_t materialID = 0;
};

struct WMOMaterialInfo {
	uint8_t flags = 0;
	uint8_t materialID = 0;
};

// -----------------------------------------------------------------------
// WMOLoader class
// -----------------------------------------------------------------------

class WMOLoader {
public:
	/**
	 * Construct a new WMOLoader instance.
	 * @param data
	 * @param fileID File name or fileDataID
	 * @param renderingOnly
	 */
	WMOLoader(BufferWrapper& data, uint32_t fileID = 0, bool renderingOnly = false);
	WMOLoader(BufferWrapper& data, const std::string& fileName, bool renderingOnly = false);

	/**
	 * Load the WMO object.
	 */
	void load();

	/**
	 * Get a group from this WMO.
	 * @param index
	 */
	WMOLoader& getGroup(uint32_t index);

	bool loaded = false;
	bool renderingOnly = false;

	// JS uses `if (!this.groups)` to detect "MOHD never parsed" (root WMO
	// asked for a group). C++ std::vector::empty() is also true when
	// groupCount == 0, which would erroneously trip on a valid zero-group
	// WMO. Track MOHD parse separately to mirror JS truthy semantics.
	bool groupsInitialized = false;

	uint32_t fileDataID = 0;
	std::string fileName;

	// MVER
	uint32_t version = 0;

	// MOHD (Header)
	uint32_t materialCount = 0;
	uint32_t groupCount = 0;
	uint32_t portalCount = 0;
	uint32_t lightCount = 0;
	uint32_t modelCount = 0;
	uint32_t doodadCount = 0;
	uint32_t setCount = 0;
	uint32_t ambientColor = 0;
	uint32_t wmoID = 0;
	std::vector<float> boundingBox1;
	std::vector<float> boundingBox2;
	uint16_t flags = 0;
	uint16_t lodCount = 0;

	// MOTX (Textures)
	// JS: `!!wmo.textureNames` (truthiness) — true even for empty chunk.
	// C++: track presence of MOTX chunk separately so isClassic check matches JS.
	bool hasMotxChunk = false;
	std::map<uint32_t, std::string> textureNames;

	// MFOG (Fog)
	std::vector<WMOFog> fogs;

	// MOMT (Materials)
	std::vector<WMOMaterial> materials;

	// MOPV (Portal Vertices)
	std::vector<std::vector<float>> portalVertices;

	// MOPT (Portal Triangles)
	std::vector<WMOPortalInfo> portalInfo;

	// MOPR (Portal References)
	std::vector<WMOPortalRef> mopr;

	// MOGN (Group Names)
	std::map<uint32_t, std::string> groupNames;

	// MOGI (Group Info)
	std::vector<WMOGroupInfo> groupInfo;

	// MODS (Doodad Sets)
	std::vector<WMODoodadSet> doodadSets;

	// MODI (Doodad IDs)
	std::vector<uint32_t> fileDataIDs;

	// MODN (Doodad Names)
	std::map<uint32_t, std::string> doodadNames;

	// MODD (Doodad Definitions)
	std::vector<WMODoodad> doodads;

	// GFID (Group file Data IDs)
	std::vector<uint32_t> groupIDs;

	// MLIQ (Liquid Data)
	WMOLiquid liquid;
	bool hasLiquid = false;

	// MOCV (Vertex Colouring)
	std::vector<std::vector<uint8_t>> vertexColours;

	// MOGP (Group Header)
	uint32_t nameOfs = 0;
	uint32_t descOfs = 0;
	// flags already declared above as uint16_t; group flags reuse this as uint32_t
	uint32_t groupFlags = 0;
	uint16_t ofsPortals = 0;
	uint16_t numPortals = 0;
	uint16_t numBatchesA = 0;
	uint16_t numBatchesB = 0;
	uint32_t numBatchesC = 0;
	uint32_t liquidType = 0;
	uint32_t groupID = 0;

	// MOVI (indices)
	std::vector<uint16_t> indices;

	// MOVT (vertices)
	std::vector<float> vertices;

	// MOTV (UVs)
	std::vector<std::vector<float>> uvs;

	// MONR (Normals)
	std::vector<float> normals;

	// MOBA (Render Batches)
	std::vector<WMORenderBatch> renderBatches;

	// MOPY (Material Info)
	std::vector<WMOMaterialInfo> materialInfo;

	// MOC2 (Colors 2)
	std::vector<uint8_t> colors2;

	// Groups (loaded on demand)
	std::vector<WMOLoader*> groups;

private:
	BufferWrapper* data;

	// Owned group loaders
	std::vector<std::unique_ptr<BufferWrapper>> ownedGroupBuffers;
	std::vector<std::unique_ptr<WMOLoader>> ownedGroups;

	void handleChunk(uint32_t chunkID, BufferWrapper& data, uint32_t chunkSize);

	void parse_MVER(BufferWrapper& data);
	void parse_MOHD(BufferWrapper& data);
	void parse_MOTX(BufferWrapper& data, uint32_t chunkSize);
	void parse_MFOG(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOMT(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOPV(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOPT(BufferWrapper& data);
	void parse_MOPR(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOGN(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOGI(BufferWrapper& data, uint32_t chunkSize);
	void parse_MODS(BufferWrapper& data, uint32_t chunkSize);
	void parse_MODI(BufferWrapper& data, uint32_t chunkSize);
	void parse_MODN(BufferWrapper& data, uint32_t chunkSize);
	void parse_MODD(BufferWrapper& data, uint32_t chunkSize);
	void parse_GFID(BufferWrapper& data, uint32_t chunkSize);
	void parse_MLIQ(BufferWrapper& data);
	void parse_MOCV(BufferWrapper& data, uint32_t chunkSize);
	void parse_MDAL(BufferWrapper& data);
	void parse_MOGP(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOVI(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOVT(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOTV(BufferWrapper& data, uint32_t chunkSize);
	void parse_MONR(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOBA(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOPY(BufferWrapper& data, uint32_t chunkSize);
	void parse_MOC2(BufferWrapper& data, uint32_t chunkSize);

	static bool isOptionalChunk(uint32_t chunkID);
};
