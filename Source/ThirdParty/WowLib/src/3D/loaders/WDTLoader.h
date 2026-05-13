/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class BufferWrapper;

struct WDTEntry {
	uint32_t rootADT;
	uint32_t obj0ADT;
	uint32_t obj1ADT;
	uint32_t tex0ADT;
	uint32_t lodADT;
	uint32_t mapTexture;
	uint32_t mapTextureN;
	uint32_t minimapTexture;
};

struct WDTWorldModelPlacement {
	uint32_t id;
	uint32_t uid;
	std::vector<float> position;
	std::vector<float> rotation;
	std::vector<float> upperExtents;
	std::vector<float> lowerExtents;
	uint16_t flags;
	uint16_t doodadSetIndex;
	uint16_t nameSet;
	uint16_t scale;
};

class WDTLoader {
public:
	/**
	 * Construct a new WDTLoader instance.
	 * @param data 
	 */
	explicit WDTLoader(BufferWrapper& data);

	/**
	 * Load the WDT file, parsing it.
	 */
	void load();

	// MPHD (Flags)
	uint32_t flags = 0;
	uint32_t lgtFileDataID = 0;
	uint32_t occFileDataID = 0;
	uint32_t fogsFileDataID = 0;
	uint32_t mpvFileDataID = 0;
	uint32_t texFileDataID = 0;
	uint32_t wdlFileDataID = 0;
	uint32_t pd4FileDataID = 0;

	// MAIN (Tiles)
	std::vector<uint32_t> tiles;

	// MAID (File IDs)
	std::vector<WDTEntry> entries;

	// MWMO (World WMO)
	std::string worldModel;

	// MODF (World WMO Placement)
	// hasWorldModelPlacement is true only when the MODF chunk was present (mirrors JS worldModelPlacement !== undefined).
	bool hasWorldModelPlacement = false;
	WDTWorldModelPlacement worldModelPlacement;

private:
	BufferWrapper& data;

	void parse_chunk_mphd(BufferWrapper& data);
	void parse_chunk_main(BufferWrapper& data);
	void parse_chunk_maid(BufferWrapper& data);
	void parse_chunk_mwmo(BufferWrapper& data, uint32_t chunkSize);
	void parse_chunk_modf(BufferWrapper& data);
};
