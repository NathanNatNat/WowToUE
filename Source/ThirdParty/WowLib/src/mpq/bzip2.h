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
 * Decompress BZip2-encoded data.
 *
 * JS equivalent: function bzip2_decompress — module.exports = { bzip2_decompress }
 *
 * @param compressed_data The BZip2-compressed data buffer.
 * @param expected_length Optional hint for expected decompressed size (0 = unknown).
 * @return Decompressed data as a byte vector.
 */
std::vector<uint8_t> bzip2_decompress(std::span<const uint8_t> compressed_data, size_t expected_length = 0);

} // namespace mpq
