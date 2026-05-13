/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

class BufferWrapper;

struct TEXEntry {
	uint32_t txmd_offset;
	uint8_t size_x;
	uint8_t size_y;
	uint8_t num_levels;
	uint8_t dxt_type;
};

struct TEXTexture {
	std::vector<uint8_t> pixels;
	uint8_t width;
	uint8_t height;
};

class TEXLoader {
public:
	explicit TEXLoader(BufferWrapper& data);

	void load();

	TEXTexture* get_texture(uint32_t file_id);

	void dispose();

	uint32_t version = 0;
	std::unordered_map<uint32_t, TEXEntry> entries;

private:
	void _parse_txbt(BufferWrapper& data, uint32_t chunk_size);

	BufferWrapper* data;
	size_t _base_offset = 0;
	std::unordered_map<uint32_t, TEXTexture> _decoded_cache;
};
