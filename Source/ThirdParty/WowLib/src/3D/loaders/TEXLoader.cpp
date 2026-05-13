/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "TEXLoader.h"
#include "DXTDecoder.h"
#include "../../buffer.h"

#include <algorithm>
#include <array>

static constexpr uint32_t CHUNK_TXVR = 0x54585652;
static constexpr uint32_t CHUNK_TXBT = 0x54584254;
static constexpr uint32_t CHUNK_TXFN = 0x5458464E;
static constexpr uint32_t CHUNK_TXMD = 0x54584D44;

static constexpr std::array<int, 3> DXT_BLOCK_BYTES = { 8, 16, 16 }; // DXT1, DXT3, DXT5

TEXLoader::TEXLoader(BufferWrapper& data)
	: data(&data) {
}

void TEXLoader::load() {
	while (this->data->remainingBytes() > 0) {
		const uint32_t chunk_id = this->data->readUInt32LE();
		const uint32_t chunk_size = this->data->readUInt32LE();
		const size_t next_pos = this->data->offset() + chunk_size;

		if (chunk_id == CHUNK_TXVR)
			this->version = this->data->readUInt32LE();
		else if (chunk_id == CHUNK_TXBT)
			this->_parse_txbt(*this->data, chunk_size);

		// txmd_offset is relative to end of TXFN (v0) or TXBT (v1+)
		if (chunk_id == CHUNK_TXBT || chunk_id == CHUNK_TXFN)
			this->_base_offset = next_pos;

		this->data->seek(next_pos);
	}
}

void TEXLoader::_parse_txbt(BufferWrapper& data, uint32_t chunk_size) {
	const uint32_t count = chunk_size / 12;

	for (uint32_t i = 0; i < count; i++) {
		const uint32_t file_id = data.readUInt32LE();
		const uint32_t txmd_offset = data.readUInt32LE();
		const uint8_t size_x = data.readUInt8();
		const uint8_t size_y = data.readUInt8();
		const uint8_t level_byte = data.readUInt8();
		const uint8_t fmt_byte = data.readUInt8();

		const uint8_t num_levels = level_byte & 0x7F;
		const uint8_t dxt_type = fmt_byte & 0x0F;

		if (file_id > 0) {
			this->entries[file_id] = TEXEntry{
				txmd_offset,
				size_x,
				size_y,
				num_levels,
				dxt_type
			};
		}
	}
}

TEXTexture* TEXLoader::get_texture(uint32_t file_id) {
	auto cached_it = this->_decoded_cache.find(file_id);
	if (cached_it != this->_decoded_cache.end())
		return &cached_it->second;

	auto entry_it = this->entries.find(file_id);
	if (entry_it == this->entries.end() || this->_base_offset == 0)
		return nullptr;

	const TEXEntry& entry = entry_it->second;
	const uint8_t size_x = entry.size_x;
	const uint8_t size_y = entry.size_y;
	const uint8_t dxt_type = entry.dxt_type;
	const uint32_t txmd_offset = entry.txmd_offset;

	if (size_x == 0 || size_y == 0)
		return nullptr;

	const int block_bytes = (dxt_type < DXT_BLOCK_BYTES.size()) ? DXT_BLOCK_BYTES[dxt_type] : 8;
	const int blocks_x = std::max(1, (size_x + 3) >> 2);
	const int blocks_y = std::max(1, (size_y + 3) >> 2);
	const size_t mip0_size = static_cast<size_t>(blocks_x) * blocks_y * block_bytes;

	// txmd_offset points to the TXMD chunk header, +8 skips id+size
	const size_t abs_offset = this->_base_offset + txmd_offset + 8;
	this->data->seek(abs_offset);

	const std::vector<uint8_t> raw = this->data->readBufferRaw(mip0_size);
	std::vector<uint8_t> pixels = dxt::decode_dxt(raw.data(), size_x, size_y, dxt_type);

	TEXTexture result;
	result.pixels = std::move(pixels);
	result.width = size_x;
	result.height = size_y;

	auto [it, _] = this->_decoded_cache.emplace(file_id, std::move(result));
	return &it->second;
}

void TEXLoader::dispose() {
	this->_decoded_cache.clear();
	this->entries.clear();
	this->data = nullptr;
}
