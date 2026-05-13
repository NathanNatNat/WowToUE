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

#include "WMOLoader.h"

class BufferWrapper;

// Version constants
static constexpr uint32_t WMO_VER_ALPHA = 14;       // 0.5.5 alpha
static constexpr uint32_t WMO_VER_PRE_VANILLA = 16;  // 0.6.0 - pre-vanilla
static constexpr uint32_t WMO_VER_VANILLA = 17;      // vanilla onwards

// Alpha group info for inline groups
struct WMOAlphaGroupEntry {
	size_t offset = 0;
	uint32_t size = 0;
};

class WMOLegacyLoader {
public:
	/**
	 * Construct a new WMOLegacyLoader instance.
	 * @param data
	 * @param fileID File name or fileDataID
	 * @param renderingOnly
	 */
	WMOLegacyLoader(BufferWrapper& data, uint32_t fileID = 0, bool renderingOnly = false);
	WMOLegacyLoader(BufferWrapper& data, const std::string& fileName, bool renderingOnly = false);

	/**
	 * Load the WMO object.
	 */
	void load();

	/**
	 * Get a group from this WMO.
	 * @param index
	 */
	WMOLegacyLoader& getGroup(uint32_t index);

	bool loaded = false;
	bool renderingOnly = false;
	// JS: `if (!this.groups)` checks if MOHD was ever parsed (groups property set).
	// C++ uses this flag to distinguish "MOHD not yet parsed" from "groupCount == 0".
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
	uint32_t groupFlags = 0;
	uint32_t ofsPortals = 0;
	uint32_t numPortals = 0;
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

	// Alpha inline group info
	std::vector<WMOAlphaGroupEntry> alphaGroups;

	// Groups (loaded on demand)
	std::vector<WMOLegacyLoader*> groups;

private:
	BufferWrapper* data;

	// Owned group data
	std::vector<std::unique_ptr<BufferWrapper>> ownedGroupBuffers;
	std::vector<std::unique_ptr<WMOLegacyLoader>> ownedGroups;

	void _load_alpha_format();
	void _load_standard_format();
	void _parse_alpha_group(BufferWrapper& data, uint32_t chunkSize);

	void handleChunk(uint32_t chunkID, BufferWrapper& data, uint32_t chunkSize);

	void parse_MVER(BufferWrapper& data);
	void parse_MOHD(BufferWrapper& data, uint32_t chunkSize);
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
