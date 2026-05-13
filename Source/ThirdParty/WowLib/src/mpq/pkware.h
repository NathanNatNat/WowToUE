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
 * Decompress PKWare DCL Implode compressed data.
 *
 * JS equivalent: function pkware_dcl_explode — module.exports = { pkware_dcl_explode }
 *
 * @param compressedData The compressed data buffer.
 * @param expectedLength Expected decompressed size.
 * @return Decompressed data as a byte vector.
 */
std::vector<uint8_t> pkware_dcl_explode(std::span<const uint8_t> compressedData, size_t expectedLength);

} // namespace mpq
