/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "ADTLoader.h"
#include "LoaderGenerics.h"
#include "WDTLoader.h"
#include "../../buffer.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <set>
#include <stdexcept>

static constexpr uint32_t CHUNK_MVER = 0x4D564552;
static constexpr uint32_t CHUNK_MCNK = 0x4D434E4B;
static constexpr uint32_t CHUNK_MH2O = 0x4D48324F;
static constexpr uint32_t CHUNK_MHDR = 0x4D484452;

static constexpr uint32_t CHUNK_MCVT = 0x4D435654;
static constexpr uint32_t CHUNK_MCCV = 0x4D434356;
static constexpr uint32_t CHUNK_MCNR = 0x4D434E52;
static constexpr uint32_t CHUNK_MCBB = 0x4D434242;

static constexpr uint32_t CHUNK_MTEX = 0x4D544558;
static constexpr uint32_t CHUNK_MTXP = 0x4D545850;
static constexpr uint32_t CHUNK_MHID = 0x4D484944;
static constexpr uint32_t CHUNK_MDID = 0x4D444944;

static constexpr uint32_t CHUNK_MCLY = 0x4D434C59;
static constexpr uint32_t CHUNK_MCAL = 0x4D43414C;

static constexpr uint32_t CHUNK_MMDX = 0x4D4D4458;
static constexpr uint32_t CHUNK_MMID = 0x4D4D4944;
static constexpr uint32_t CHUNK_MWMO = 0x4D574D4F;
static constexpr uint32_t CHUNK_MWID = 0x4D574944;
static constexpr uint32_t CHUNK_MDDF = 0x4D444446;
static constexpr uint32_t CHUNK_MODF = 0x4D4F4446;
static constexpr uint32_t CHUNK_MWDS = 0x4D574453;

/**
 * Construct a new ADTLoader instance.
 * @param data
 */
ADTLoader::ADTLoader(BufferWrapper& data)
	: data(data) {
}

/**
 * Parse this ADT as a root file.
 */
void ADTLoader::loadRoot() {
	this->chunks.resize(16 * 16);
	this->chunkIndex = 0;

	this->_load_root();
}

/**
 * Parse this ADT as an object file.
 */
void ADTLoader::loadObj() {
	this->_load_obj();
}

/**
 * Parse this ADT as a texture file.
 * @param wdt
 */
void ADTLoader::loadTex(WDTLoader& wdt) {
	this->texChunks.resize(16 * 16);
	this->chunkIndex = 0;
	this->wdt = &wdt;

	this->_load_tex();
}

void ADTLoader::_load_root() {
	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		switch (chunkID) {
			case CHUNK_MVER: this->handle_root_MVER(this->data); break;
			case CHUNK_MCNK: this->handle_root_MCNK(this->data, chunkSize); break;
			case CHUNK_MH2O: this->handle_root_MH2O(this->data, chunkSize); break;
			case CHUNK_MHDR: this->handle_root_MHDR(this->data); break;
		}

		// Ensure that we start at the next chunk exactly.
		this->data.seek(nextChunkPos);
	}
}

void ADTLoader::_load_obj() {
	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		switch (chunkID) {
			case CHUNK_MVER: this->handle_obj_MVER(this->data); break;
			case CHUNK_MMDX: this->handle_obj_MMDX(this->data, chunkSize); break;
			case CHUNK_MMID: this->handle_obj_MMID(this->data, chunkSize); break;
			case CHUNK_MWMO: this->handle_obj_MWMO(this->data, chunkSize); break;
			case CHUNK_MWID: this->handle_obj_MWID(this->data, chunkSize); break;
			case CHUNK_MDDF: this->handle_obj_MDDF(this->data, chunkSize); break;
			case CHUNK_MODF: this->handle_obj_MODF(this->data, chunkSize); break;
			case CHUNK_MWDS: this->handle_obj_MWDS(this->data, chunkSize); break;
		}

		// Ensure that we start at the next chunk exactly.
		this->data.seek(nextChunkPos);
	}
}

void ADTLoader::_load_tex() {
	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		switch (chunkID) {
			case CHUNK_MVER: this->handle_tex_MVER(this->data); break;
			case CHUNK_MTEX: this->handle_tex_MTEX(this->data, chunkSize); break;
			case CHUNK_MCNK: this->handle_tex_MCNK(this->data, chunkSize); break;
			case CHUNK_MTXP: this->handle_tex_MTXP(this->data, chunkSize); break;
			case CHUNK_MHID: this->handle_tex_MHID(this->data, chunkSize); break;
			case CHUNK_MDID: this->handle_tex_MDID(this->data, chunkSize); break;
		}

		// Ensure that we start at the next chunk exactly.
		this->data.seek(nextChunkPos);
	}
}

// MVER (Version)
void ADTLoader::handle_root_MVER(BufferWrapper& data) {
	this->version = data.readUInt32LE();
	if (this->version != 18)
		throw std::runtime_error("Unexpected ADT version: " + std::to_string(this->version));
}

// MCNK
void ADTLoader::handle_root_MCNK(BufferWrapper& data, uint32_t chunkSize) {
	const size_t endOfs = data.offset() + chunkSize;
	ADTChunk& chunk = this->chunks[this->chunkIndex++];

	chunk.flags = data.readUInt32LE();
	chunk.indexX = data.readUInt32LE();
	chunk.indexY = data.readUInt32LE();
	chunk.nLayers = data.readUInt32LE();
	chunk.nDoodadRefs = data.readUInt32LE();
	chunk.holesHighRes = data.readUInt8(8);
	chunk.ofsMCLY = data.readUInt32LE();
	chunk.ofsMCRF = data.readUInt32LE();
	chunk.ofsMCAL = data.readUInt32LE();
	chunk.sizeAlpha = data.readUInt32LE();
	chunk.ofsMCSH = data.readUInt32LE();
	chunk.sizeShadows = data.readUInt32LE();
	chunk.areaID = data.readUInt32LE();
	chunk.nMapObjRefs = data.readUInt32LE();
	chunk.holesLowRes = data.readUInt16LE();
	chunk.unk1 = data.readUInt16LE();
	chunk.lowQualityTextureMap = data.readInt16LE(8);
	chunk.noEffectDoodad = data.readInt64LE();
	chunk.ofsMCSE = data.readUInt32LE();
	chunk.numMCSE = data.readUInt32LE();
	chunk.ofsMCLQ = data.readUInt32LE();
	chunk.sizeMCLQ = data.readUInt32LE();
	chunk.position = data.readFloatLE(3);
	chunk.ofsMCCV = data.readUInt32LE();
	chunk.ofsMCLW = data.readUInt32LE();
	chunk.unk2 = data.readUInt32LE();

	// Read sub-chunks.
	while (data.offset() < endOfs) {
		const uint32_t subChunkID = data.readUInt32LE();
		const uint32_t subChunkSize = data.readUInt32LE();
		const size_t nextChunkPos = data.offset() + subChunkSize;

		switch (subChunkID) {
			case CHUNK_MCVT: handle_mcnk_MCVT(chunk, data); break;
			case CHUNK_MCCV: handle_mcnk_MCCV(chunk, data); break;
			case CHUNK_MCNR: handle_mcnk_MCNR(chunk, data); break;
			case CHUNK_MCBB: handle_mcnk_MCBB(chunk, data, subChunkSize); break;
		}

		// Ensure that we start at the next chunk exactly.
		data.seek(nextChunkPos);
	}
}

// MH2O (Liquids)
void ADTLoader::handle_root_MH2O(BufferWrapper& data, uint32_t chunkSize) {
	const size_t base = data.offset();
	const size_t chunkEnd = base + chunkSize;
	std::set<uint32_t> dataOffsets;

	struct MH2OChunkHeader {
		uint32_t offsetInstances;
		uint32_t layerCount;
		uint32_t offsetAttributes;
	};

	std::vector<MH2OChunkHeader> chunkHeaders(256);
	this->liquidChunks.resize(256);

	for (int i = 0; i < 256; i++) {
		chunkHeaders[i].offsetInstances = data.readUInt32LE();
		chunkHeaders[i].layerCount = data.readUInt32LE();
		chunkHeaders[i].offsetAttributes = data.readUInt32LE();

		if (chunkHeaders[i].offsetAttributes > 0)
			dataOffsets.insert(chunkHeaders[i].offsetAttributes);

		this->liquidChunks[i].attributes = { 0, 0 };
		this->liquidChunks[i].instances.resize(chunkHeaders[i].layerCount);
	}

	std::vector<LiquidInstance*> allInstances;
	for (int i = 0; i < 256; i++) {
		const auto& header = chunkHeaders[i];
		auto& lchunk = this->liquidChunks[i];

		if (header.layerCount > 0) {
			data.seek(base + header.offsetInstances);

			for (uint32_t j = 0; j < header.layerCount; j++) {
				LiquidInstance& instance = lchunk.instances[j];
				instance.chunkIndex = i;
				instance.instanceIndex = static_cast<int>(j);
				instance.liquidType = data.readUInt16LE();
				instance.liquidObject = data.readUInt16LE();
				instance.minHeightLevel = data.readFloatLE();
				instance.maxHeightLevel = data.readFloatLE();
				instance.xOffset = data.readUInt8();
				instance.yOffset = data.readUInt8();
				instance.width = data.readUInt8();
				instance.height = data.readUInt8();
				instance.offsetExistsBitmap = data.readUInt32LE();
				instance.offsetVertexData = data.readUInt32LE();

				// default values for liquidObject <= 41
				if (instance.liquidObject <= 41) {
					instance.xOffset = 0;
					instance.yOffset = 0;
					instance.width = 8;
					instance.height = 8;
				}

				if (instance.offsetExistsBitmap > 0)
					dataOffsets.insert(instance.offsetExistsBitmap);

				if (instance.offsetVertexData > 0)
					dataOffsets.insert(instance.offsetVertexData);

				allInstances.push_back(&instance);
			}
		}
	}

	std::vector<uint32_t> sortedOffsets(dataOffsets.begin(), dataOffsets.end());

	for (int i = 0; i < 256; i++) {
		const auto& header = chunkHeaders[i];
		auto& lchunk = this->liquidChunks[i];

		if (header.offsetAttributes > 0) {
			data.seek(base + header.offsetAttributes);
			lchunk.attributes.fishable = data.readUInt64LE();
			lchunk.attributes.deep = data.readUInt64LE();
		}
	}

	for (LiquidInstance* instance : allInstances) {
		if (instance->offsetExistsBitmap > 0) {
			data.seek(base + instance->offsetExistsBitmap);
			const uint32_t bitmapSize = static_cast<uint32_t>(std::ceil((instance->width * instance->height + 7) / 8.0));
			instance->bitmap = data.readUInt8(bitmapSize);
		}

		// Handle special case: if offsetVertexData is 0, no vertex data in file
		if (instance->offsetVertexData == 0) {
			const int vertexCount = (instance->width + 1) * (instance->height + 1);
			// ocean liquid (type 2) with no vertex data: flat surface at sea level or min/max average
			const float waterLevel = (instance->minHeightLevel == 0.0f && instance->maxHeightLevel == 0.0f)
				? 0.0f : (instance->minHeightLevel + instance->maxHeightLevel) / 2.0f;
			instance->vertexData.height.assign(vertexCount, waterLevel);
		} else if (instance->offsetVertexData > 0) {
			const int vertexCount = (instance->width + 1) * (instance->height + 1);

			auto it = std::find(sortedOffsets.begin(), sortedOffsets.end(), instance->offsetVertexData);
			size_t offsetIndex = static_cast<size_t>(std::distance(sortedOffsets.begin(), it));
			uint32_t dataSize;

			// Calculate data size using next offset
			if (offsetIndex < sortedOffsets.size() - 1) {
				dataSize = sortedOffsets[offsetIndex + 1] - instance->offsetVertexData;
			} else {
				// last data block: use remaining bytes in chunk
				dataSize = static_cast<uint32_t>(chunkEnd - (base + instance->offsetVertexData));
			}

			data.seek(base + instance->offsetVertexData);

			const int bytesPerVertex = static_cast<int>(dataSize) / vertexCount;

			if (bytesPerVertex == 5) {
				// Case 0: Height + Depth (5 bytes per vertex)
				instance->vertexData.height = data.readFloatLE(vertexCount);
				instance->vertexData.depth = data.readUInt8(vertexCount);
			} else if (bytesPerVertex == 8) {
				// Case 1: Height + UV (8 bytes per vertex)
				instance->vertexData.height = data.readFloatLE(vertexCount);
				instance->vertexData.uv.resize(vertexCount);
				for (int vi = 0; vi < vertexCount; vi++) {
					instance->vertexData.uv[vi] = {
						data.readUInt16LE(),
						data.readUInt16LE()
					};
				}
			} else if (bytesPerVertex == 1) {
				// Case 2: Depth only (1 byte per vertex)
				instance->vertexData.depth = data.readUInt8(vertexCount);
				// ocean liquid: height is constant at minHeightLevel (typically 0.0 for sea level)
				const float waterLevel = (instance->minHeightLevel == 0.0f && instance->maxHeightLevel == 0.0f)
					? 0.0f : (instance->minHeightLevel + instance->maxHeightLevel) / 2.0f;
				instance->vertexData.height.assign(vertexCount, waterLevel);
			} else if (bytesPerVertex == 9) {
				// Case 3: Height + UV + Depth (9 bytes per vertex)
				instance->vertexData.height = data.readFloatLE(vertexCount);
				instance->vertexData.uv.resize(vertexCount);
				for (int vi = 0; vi < vertexCount; vi++) {
					instance->vertexData.uv[vi] = {
						data.readUInt16LE(),
						data.readUInt16LE()
					};
				}
				instance->vertexData.depth = data.readUInt8(vertexCount);
			}
		}
	}
}

// MHDR (Header)
void ADTLoader::handle_root_MHDR(BufferWrapper& data) {
	this->header.flags = data.readUInt32LE();
	this->header.ofsMCIN = data.readUInt32LE();
	this->header.ofsMTEX = data.readUInt32LE();
	this->header.ofsMMDX = data.readUInt32LE();
	this->header.ofsMMID = data.readUInt32LE();
	this->header.ofsMWMO = data.readUInt32LE();
	this->header.ofsMWID = data.readUInt32LE();
	this->header.ofsMDDF = data.readUInt32LE();
	this->header.ofsMODF = data.readUInt32LE();
	this->header.ofsMFBO = data.readUInt32LE();
	this->header.ofsMH20 = data.readUInt32LE();
	this->header.ofsMTXF = data.readUInt32LE();
	this->header.unk = data.readUInt32LE(4);
}

// MCVT (vertices)
void ADTLoader::handle_mcnk_MCVT(ADTChunk& chunk, BufferWrapper& data) {
	chunk.vertices = data.readFloatLE(145);
}

// MCCV (Vertex Shading)
void ADTLoader::handle_mcnk_MCCV(ADTChunk& chunk, BufferWrapper& data) {
	chunk.vertexShading.resize(145);
	for (int i = 0; i < 145; i++) {
		chunk.vertexShading[i] = {
			data.readUInt8(),
			data.readUInt8(),
			data.readUInt8(),
			data.readUInt8()
		};
	}
}

// MCNR (Normals)
void ADTLoader::handle_mcnk_MCNR(ADTChunk& chunk, BufferWrapper& data) {
	chunk.normals.resize(145);
	for (int i = 0; i < 145; i++) {
		const int8_t x = data.readInt8();
		const int8_t z = data.readInt8();
		const int8_t y = data.readInt8();

		chunk.normals[i] = { x, y, z };
	}
}

// MCBB (Blend Batches)
void ADTLoader::handle_mcnk_MCBB(ADTChunk& chunk, BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 20;
	chunk.blendBatches.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		chunk.blendBatches[i] = {
			data.readUInt32LE(),
			data.readUInt32LE(),
			data.readUInt32LE(),
			data.readUInt32LE(),
			data.readUInt32LE()
		};
	}
}

// MVER (Version)
void ADTLoader::handle_tex_MVER(BufferWrapper& data) {
	this->version = data.readUInt32LE();
	if (this->version != 18)
		throw std::runtime_error("Unexpected ADT version: " + std::to_string(this->version));
}

// MTEX (Textures)
void ADTLoader::handle_tex_MTEX(BufferWrapper& data, uint32_t chunkSize) {
	this->textures = ReadStringBlock(data, chunkSize);
}

// MCNK (Texture Chunks)
void ADTLoader::handle_tex_MCNK(BufferWrapper& data, uint32_t chunkSize) {
	const size_t endOfs = data.offset() + chunkSize;
	ADTTexChunk& chunk = this->texChunks[this->chunkIndex++];

	// Read sub-chunks.
	while (data.offset() < endOfs) {
		const uint32_t subChunkID = data.readUInt32LE();
		const uint32_t subChunkSize = data.readUInt32LE();
		const size_t nextChunkPos = data.offset() + subChunkSize;

		switch (subChunkID) {
			case CHUNK_MCLY: handle_texmcnk_MCLY(chunk, data, subChunkSize); break;
			case CHUNK_MCAL: handle_texmcnk_MCAL(chunk, data, subChunkSize, this->wdt); break;
		}

		// Ensure that we start at the next chunk exactly.
		data.seek(nextChunkPos);
	}
}

// MTXP
void ADTLoader::handle_tex_MTXP(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 16;
	this->texParams.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		this->texParams[i] = {
			data.readUInt32LE(),
			data.readFloatLE(),
			data.readFloatLE(),
			data.readUInt32LE()
		};
	}
}

// MHID
void ADTLoader::handle_tex_MHID(BufferWrapper& data, uint32_t chunkSize) {
	this->heightTextureFileDataIDs = data.readUInt32LE(chunkSize / 4);
}

// MDID
void ADTLoader::handle_tex_MDID(BufferWrapper& data, uint32_t chunkSize) {
	this->diffuseTextureFileDataIDs = data.readUInt32LE(chunkSize / 4);
}

// MCLY
void ADTLoader::handle_texmcnk_MCLY(ADTTexChunk& chunk, BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 16;
	chunk.layers.resize(count);
	chunk.layerCount = count;

	for (uint32_t i = 0; i < count; i++) {
		chunk.layers[i] = {
			data.readUInt32LE(),
			data.readUInt32LE(),
			data.readUInt32LE(),
			data.readInt32LE()
		};
	}
}

// MCAL
void ADTLoader::handle_texmcnk_MCAL(ADTTexChunk& chunk, BufferWrapper& data, uint32_t chunkSize, WDTLoader* wdt) {
	if (wdt == nullptr)
		throw std::runtime_error("ADTLoader::handle_texmcnk_MCAL: WDT root required");

	const size_t layerCount = chunk.layers.size();
	chunk.alphaLayers.resize(layerCount);
	chunk.alphaLayers[0].assign(64 * 64, 255);

	uint32_t ofs = 0;
	for (size_t i = 1; i < layerCount; i++) {
		const auto& layer = chunk.layers[i];

		if (layer.offsetMCAL != ofs)
			throw std::runtime_error("MCAL offset mis-match");

		if (layer.flags & 0x200) {
			// Compressed.
			auto& alphaLayer = chunk.alphaLayers[i];
			alphaLayer.resize(64 * 64);

			uint32_t inOfs = 0;
			uint32_t outOfs = 0;

			while (outOfs < 4096) {
				const uint8_t info = data.readUInt8();
				inOfs++;

				const uint8_t mode = (info & 0x80) >> 7;
				uint8_t count = (info & 0x7F);

				if (mode != 0) {
					const uint8_t value = data.readUInt8();
					inOfs++;

					while (count-- > 0 && outOfs < 4096) {
						alphaLayer[outOfs] = value;
						outOfs++;
					}
				} else {
					while (count-- > 0 && outOfs < 4096) {
						const uint8_t value = data.readUInt8();
						inOfs++;

						alphaLayer[outOfs] = value;
						outOfs++;
					}
				}
			}

			ofs += inOfs;
			if (outOfs != 4096)
				throw std::runtime_error("Broken ADT.");
		} else if (wdt->flags & 0x4 || wdt->flags & 0x80) {
			// Uncompressed (4096)
			chunk.alphaLayers[i] = data.readUInt8(4096);
			ofs += 4096;
		} else {
			// Uncompressed (2048)
			auto& alphaLayer = chunk.alphaLayers[i];
			alphaLayer.resize(64 * 64);
			const auto rawLayer = data.readUInt8(2048);
			ofs += 2048;

			for (int j = 0; j < 2048; j++) {
				alphaLayer[2 * j + 0] = static_cast<uint8_t>(((rawLayer[j] & 0x0F) >> 0) * 17);
				alphaLayer[2 * j + 1] = static_cast<uint8_t>(((rawLayer[j] & 0xF0) >> 4) * 17);
			}
		}
	}
}

// MVER (Version)
void ADTLoader::handle_obj_MVER(BufferWrapper& data) {
	this->version = data.readUInt32LE();
	if (this->version != 18)
		throw std::runtime_error("Unexpected ADT version: " + std::to_string(this->version));
}

// MMDX (Doodad Filenames)
void ADTLoader::handle_obj_MMDX(BufferWrapper& data, uint32_t chunkSize) {
	this->m2Names = ReadStringBlock(data, chunkSize);
}

// MMID (M2 Offsets)
void ADTLoader::handle_obj_MMID(BufferWrapper& data, uint32_t chunkSize) {
	this->m2Offsets = data.readUInt32LE(chunkSize / 4);
}

// MWMO (WMO Filenames)
void ADTLoader::handle_obj_MWMO(BufferWrapper& data, uint32_t chunkSize) {
	this->wmoNames = ReadStringBlock(data, chunkSize);
}

// MWID (WMO Offsets)
void ADTLoader::handle_obj_MWID(BufferWrapper& data, uint32_t chunkSize) {
	this->wmoOffsets = data.readUInt32LE(chunkSize / 4);
}

// MDDF
void ADTLoader::handle_obj_MDDF(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 36;
	this->models.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		this->models[i].mmidEntry = data.readUInt32LE();
		this->models[i].uniqueId = data.readUInt32LE();
		this->models[i].position = data.readFloatLE(3);
		this->models[i].rotation = data.readFloatLE(3);
		this->models[i].scale = data.readUInt16LE();
		this->models[i].flags = data.readUInt16LE();
	}
}

// MODF
void ADTLoader::handle_obj_MODF(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 64;
	this->worldModels.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		this->worldModels[i].mwidEntry = data.readUInt32LE();
		this->worldModels[i].uniqueId = data.readUInt32LE();
		this->worldModels[i].position = data.readFloatLE(3);
		this->worldModels[i].rotation = data.readFloatLE(3);
		this->worldModels[i].lowerBounds = data.readFloatLE(3);
		this->worldModels[i].upperBounds = data.readFloatLE(3);
		this->worldModels[i].flags = data.readUInt16LE();
		this->worldModels[i].doodadSet = data.readUInt16LE();
		this->worldModels[i].nameSet = data.readUInt16LE();
		this->worldModels[i].scale = data.readUInt16LE();
	}
}

// MWDS
void ADTLoader::handle_obj_MWDS(BufferWrapper& data, uint32_t chunkSize) {
	this->doodadSets = data.readUInt16LE(chunkSize / 2);
}