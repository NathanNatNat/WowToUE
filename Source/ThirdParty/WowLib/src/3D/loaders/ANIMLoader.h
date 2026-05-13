/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>

class BufferWrapper;

// See: https://wowdev.wiki/M2#.anim_files
class ANIMLoader {
public:
	/**
	 * Construct a new ANIMLoader instance.
	 * @param data 
	 */
	explicit ANIMLoader(BufferWrapper& data);

	/**
	 * Load the animation file.
	 */
	void load(bool isChunked = true);

	bool isLoaded;
	std::vector<uint8_t> animData;
	std::vector<uint8_t> skeletonAttachmentData;
	std::vector<uint8_t> skeletonBoneData;
	bool hasSkeletonBoneData = false;

private:
	static constexpr uint32_t CHUNK_AFM2 = 0x324D4641;
	static constexpr uint32_t CHUNK_AFSA = 0x41534641;
	static constexpr uint32_t CHUNK_AFSB = 0x42534641;

	BufferWrapper& data;

	void parse_chunk_afm2(uint32_t chunkSize);
	void parse_chunk_afsa(uint32_t chunkSize);
	void parse_chunk_afsb(uint32_t chunkSize);
};
