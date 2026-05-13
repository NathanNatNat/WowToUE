/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>

class BufferWrapper;

// See: https://wowdev.wiki/BONE
class BONELoader {
public:
	/**
	 * Construct a new BONELoader instance.
	 * @param data 
	 */
	explicit BONELoader(BufferWrapper& data);

	/**
	 * Load the bone file.
	 */
	void load();

	bool isLoaded;
	std::vector<uint16_t> boneIDs;
	std::vector<std::vector<std::vector<float>>> boneOffsetMatrices;

private:
	static constexpr uint32_t CHUNK_BIDA = 0x41444942;
	static constexpr uint32_t CHUNK_BOMT = 0x544D4F42;

	BufferWrapper& data;

	void parse_chunk_bida(uint32_t chunkSize);
	void parse_chunk_bomt(uint32_t chunkSize);
};
