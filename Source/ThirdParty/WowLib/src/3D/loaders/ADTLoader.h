/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class BufferWrapper;
class WDTLoader;

// -----------------------------------------------------------------------
// ADT chunk sub-structures
// -----------------------------------------------------------------------

struct ADTVertexShading {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct ADTBlendBatch {
	uint32_t mbmhIndex;
	uint32_t indexCount;
	uint32_t indexFirst;
	uint32_t vertexCount;
	uint32_t vertexFirst;
};

struct ADTChunk {
	uint32_t flags = 0;
	uint32_t indexX = 0;
	uint32_t indexY = 0;
	uint32_t nLayers = 0;
	uint32_t nDoodadRefs = 0;
	std::vector<uint8_t> holesHighRes;
	uint32_t ofsMCLY = 0;
	uint32_t ofsMCRF = 0;
	uint32_t ofsMCAL = 0;
	uint32_t sizeAlpha = 0;
	uint32_t ofsMCSH = 0;
	uint32_t sizeShadows = 0;
	uint32_t areaID = 0;
	uint32_t nMapObjRefs = 0;
	uint16_t holesLowRes = 0;
	uint16_t unk1 = 0;
	std::vector<int16_t> lowQualityTextureMap;
	int64_t noEffectDoodad = 0;
	uint32_t ofsMCSE = 0;
	uint32_t numMCSE = 0;
	uint32_t ofsMCLQ = 0;
	uint32_t sizeMCLQ = 0;
	std::vector<float> position;
	uint32_t ofsMCCV = 0;
	uint32_t ofsMCLW = 0;
	uint32_t unk2 = 0;

	// Sub-chunk data (MCVT, MCCV, MCNR, MCBB)
	std::vector<float> vertices;
	std::vector<ADTVertexShading> vertexShading;
	std::vector<std::vector<int8_t>> normals;
	std::vector<ADTBlendBatch> blendBatches;
};

// -----------------------------------------------------------------------
// ADT header
// -----------------------------------------------------------------------

struct ADTHeader {
	uint32_t flags = 0;
	uint32_t ofsMCIN = 0;
	uint32_t ofsMTEX = 0;
	uint32_t ofsMMDX = 0;
	uint32_t ofsMMID = 0;
	uint32_t ofsMWMO = 0;
	uint32_t ofsMWID = 0;
	uint32_t ofsMDDF = 0;
	uint32_t ofsMODF = 0;
	uint32_t ofsMFBO = 0;
	uint32_t ofsMH20 = 0;
	uint32_t ofsMTXF = 0;
	std::vector<uint32_t> unk;
};

// -----------------------------------------------------------------------
// Liquid structures (MH2O)
// -----------------------------------------------------------------------

struct LiquidUV {
	uint16_t x;
	uint16_t y;
};

struct LiquidVertexData {
	std::vector<float> height;
	std::vector<uint8_t> depth;
	std::vector<LiquidUV> uv;
};

struct LiquidInstance {
	int chunkIndex = 0;
	int instanceIndex = 0;
	uint16_t liquidType = 0;
	uint16_t liquidObject = 0;
	float minHeightLevel = 0.0f;
	float maxHeightLevel = 0.0f;
	uint8_t xOffset = 0;
	uint8_t yOffset = 0;
	uint8_t width = 0;
	uint8_t height = 0;
	std::vector<uint8_t> bitmap;
	LiquidVertexData vertexData;
	uint32_t offsetExistsBitmap = 0;
	uint32_t offsetVertexData = 0;
};

struct LiquidAttributes {
	uint64_t fishable = 0;
	uint64_t deep = 0;
};

struct LiquidChunk {
	LiquidAttributes attributes;
	std::vector<LiquidInstance> instances;
};

// -----------------------------------------------------------------------
// Texture chunk structures
// -----------------------------------------------------------------------

struct ADTTexLayer {
	uint32_t textureId = 0;
	uint32_t flags = 0;
	uint32_t offsetMCAL = 0;
	int32_t effectID = 0;
};

struct ADTTexChunk {
	std::vector<ADTTexLayer> layers;
	uint32_t layerCount = 0;
	std::vector<std::vector<uint8_t>> alphaLayers;
};

struct ADTTexParam {
	uint32_t flags = 0;
	float height = 0.0f;
	float offset = 0.0f;
	uint32_t unk3 = 0;
};

// -----------------------------------------------------------------------
// Object chunk structures (MDDF / MODF)
// -----------------------------------------------------------------------

struct ADTModelEntry {
	uint32_t mmidEntry = 0;
	uint32_t uniqueId = 0;
	std::vector<float> position;
	std::vector<float> rotation;
	uint16_t scale = 0;
	uint16_t flags = 0;
};

struct ADTWorldModelEntry {
	uint32_t mwidEntry = 0;
	uint32_t uniqueId = 0;
	std::vector<float> position;
	std::vector<float> rotation;
	std::vector<float> lowerBounds;
	std::vector<float> upperBounds;
	uint16_t flags = 0;
	uint16_t doodadSet = 0;
	uint16_t nameSet = 0;
	uint16_t scale = 0;
};

// -----------------------------------------------------------------------
// ADT Loader class
// -----------------------------------------------------------------------

class ADTLoader {
public:
	/**
	 * Construct a new ADTLoader instance.
	 * @param data
	 */
	explicit ADTLoader(BufferWrapper& data);

	/**
	 * Parse this ADT as a root file.
	 */
	void loadRoot();

	/**
	 * Parse this ADT as an object file.
	 */
	void loadObj();

	/**
	 * Parse this ADT as a texture file.
	 * @param wdt
	 */
	void loadTex(WDTLoader& wdt);

	// Root data
	uint32_t version = 0;
	ADTHeader header;
	std::vector<ADTChunk> chunks;
	std::vector<LiquidChunk> liquidChunks;

	// Texture data
	std::map<uint32_t, std::string> textures;
	std::vector<ADTTexChunk> texChunks;
	std::vector<ADTTexParam> texParams;
	std::vector<uint32_t> heightTextureFileDataIDs;
	std::vector<uint32_t> diffuseTextureFileDataIDs;

	// Object data
	std::map<uint32_t, std::string> m2Names;
	std::vector<uint32_t> m2Offsets;
	std::map<uint32_t, std::string> wmoNames;
	std::vector<uint32_t> wmoOffsets;
	std::vector<ADTModelEntry> models;
	std::vector<ADTWorldModelEntry> worldModels;
	std::vector<uint16_t> doodadSets;

private:
	BufferWrapper& data;
	int chunkIndex = 0;
	WDTLoader* wdt = nullptr;

	void _load_root();
	void _load_obj();
	void _load_tex();

	// Root chunk handlers
	void handle_root_MVER(BufferWrapper& data);
	void handle_root_MCNK(BufferWrapper& data, uint32_t chunkSize);
	void handle_root_MH2O(BufferWrapper& data, uint32_t chunkSize);
	void handle_root_MHDR(BufferWrapper& data);

	// Root MCNK sub-chunk handlers
	static void handle_mcnk_MCVT(ADTChunk& chunk, BufferWrapper& data);
	static void handle_mcnk_MCCV(ADTChunk& chunk, BufferWrapper& data);
	static void handle_mcnk_MCNR(ADTChunk& chunk, BufferWrapper& data);
	static void handle_mcnk_MCBB(ADTChunk& chunk, BufferWrapper& data, uint32_t chunkSize);

	// Tex chunk handlers
	void handle_tex_MVER(BufferWrapper& data);
	void handle_tex_MTEX(BufferWrapper& data, uint32_t chunkSize);
	void handle_tex_MCNK(BufferWrapper& data, uint32_t chunkSize);
	void handle_tex_MTXP(BufferWrapper& data, uint32_t chunkSize);
	void handle_tex_MHID(BufferWrapper& data, uint32_t chunkSize);
	void handle_tex_MDID(BufferWrapper& data, uint32_t chunkSize);

	// Tex MCNK sub-chunk handlers
	static void handle_texmcnk_MCLY(ADTTexChunk& chunk, BufferWrapper& data, uint32_t chunkSize);
	static void handle_texmcnk_MCAL(ADTTexChunk& chunk, BufferWrapper& data, uint32_t chunkSize, WDTLoader* wdt);

	// Obj chunk handlers
	void handle_obj_MVER(BufferWrapper& data);
	void handle_obj_MMDX(BufferWrapper& data, uint32_t chunkSize);
	void handle_obj_MMID(BufferWrapper& data, uint32_t chunkSize);
	void handle_obj_MWMO(BufferWrapper& data, uint32_t chunkSize);
	void handle_obj_MWID(BufferWrapper& data, uint32_t chunkSize);
	void handle_obj_MDDF(BufferWrapper& data, uint32_t chunkSize);
	void handle_obj_MODF(BufferWrapper& data, uint32_t chunkSize);
	void handle_obj_MWDS(BufferWrapper& data, uint32_t chunkSize);
};
