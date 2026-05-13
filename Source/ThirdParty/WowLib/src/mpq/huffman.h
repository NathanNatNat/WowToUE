/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>

namespace mpq {

/**
 * Decompress Huffman-encoded data used in MPQ archives.
 *
 * JS equivalent: function huffman_decomp — module.exports = { huffman_decomp }
 *
 * @param compressedData The compressed data buffer (first byte = compression type).
 * @return Decompressed data as a byte vector.
 */
std::vector<uint8_t> huffman_decomp(std::span<const uint8_t> compressedData);

} // namespace mpq
