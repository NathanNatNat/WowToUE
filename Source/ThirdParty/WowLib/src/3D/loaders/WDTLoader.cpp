#include "WDTLoader.h"
#include "../../buffer.h"
#include "../../constants.h"

static constexpr int MAP_SIZE = constants::GAME::MAP_SIZE;
static constexpr int MAP_SIZE_SQ = constants::GAME::MAP_SIZE_SQ;

static constexpr uint32_t CHUNK_MPHD = 0x4D504844;
static constexpr uint32_t CHUNK_MAIN = 0x4D41494E;
static constexpr uint32_t CHUNK_MAID = 0x4D414944;
static constexpr uint32_t CHUNK_MWMO = 0x4D574D4F;
static constexpr uint32_t CHUNK_MODF = 0x4D4F4446;

/**
 * Construct a new WDTLoader instance.
 * @param data 
 */
WDTLoader::WDTLoader(BufferWrapper& data)
	: data(data) {
}

/**
 * Load the WDT file, parsing it.
 */
void WDTLoader::load() {
	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		switch (chunkID) {
			case CHUNK_MPHD: this->parse_chunk_mphd(this->data); break;
			case CHUNK_MAIN: this->parse_chunk_main(this->data); break;
			case CHUNK_MAID: this->parse_chunk_maid(this->data); break;
			case CHUNK_MWMO: this->parse_chunk_mwmo(this->data, chunkSize); break;
			case CHUNK_MODF: this->parse_chunk_modf(this->data); break;
		}

		// Ensure that we start at the next chunk exactly.
		this->data.seek(nextChunkPos);
	}
}

// MPHD (Flags)
void WDTLoader::parse_chunk_mphd(BufferWrapper& data) {
	this->flags = data.readUInt32LE();
	this->lgtFileDataID = data.readUInt32LE();
	this->occFileDataID = data.readUInt32LE();
	this->fogsFileDataID = data.readUInt32LE();
	this->mpvFileDataID = data.readUInt32LE();
	this->texFileDataID = data.readUInt32LE();
	this->wdlFileDataID = data.readUInt32LE();
	this->pd4FileDataID = data.readUInt32LE();
}

// MAIN (Tiles)
void WDTLoader::parse_chunk_main(BufferWrapper& data) {
	this->tiles.resize(MAP_SIZE_SQ);
	for (int x = 0; x < MAP_SIZE; x++) {
		for (int y = 0; y < MAP_SIZE; y++) {
			this->tiles[(y * MAP_SIZE) + x] = data.readUInt32LE();
			data.move(4);
		}
	}
}

// MAID (File IDs)
void WDTLoader::parse_chunk_maid(BufferWrapper& data) {
	this->entries.resize(MAP_SIZE_SQ);

	for (int x = 0; x < MAP_SIZE; x++) {
		for (int y = 0; y < MAP_SIZE; y++) {
			WDTEntry& entry = this->entries[(y * MAP_SIZE) + x];
			entry.rootADT = data.readUInt32LE();
			entry.obj0ADT = data.readUInt32LE();
			entry.obj1ADT = data.readUInt32LE();
			entry.tex0ADT = data.readUInt32LE();
			entry.lodADT = data.readUInt32LE();
			entry.mapTexture = data.readUInt32LE();
			entry.mapTextureN = data.readUInt32LE();
			entry.minimapTexture = data.readUInt32LE();
		}
	}
}

// MWMO (World WMO)
void WDTLoader::parse_chunk_mwmo(BufferWrapper& data, uint32_t chunkSize) {
	std::string raw = data.readString(chunkSize);
	auto null_pos = raw.find('\0');
	if (null_pos != std::string::npos)
		raw.erase(null_pos, 1);
	this->worldModel = std::move(raw);
}

// MODF (World WMO Placement)
void WDTLoader::parse_chunk_modf(BufferWrapper& data) {
	this->hasWorldModelPlacement = true;
	this->worldModelPlacement.id = data.readUInt32LE();
	this->worldModelPlacement.uid = data.readUInt32LE();
	this->worldModelPlacement.position = data.readFloatLE(3);
	this->worldModelPlacement.rotation = data.readFloatLE(3);
	this->worldModelPlacement.upperExtents = data.readFloatLE(3);
	this->worldModelPlacement.lowerExtents = data.readFloatLE(3);
	this->worldModelPlacement.flags = data.readUInt16LE();
	this->worldModelPlacement.doodadSetIndex = data.readUInt16LE();
	this->worldModelPlacement.nameSet = data.readUInt16LE();
	this->worldModelPlacement.scale = data.readUInt16LE();
}