/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>

namespace dxt {

constexpr int DXT_TYPE_DXT1 = 0;
constexpr int DXT_TYPE_DXT3 = 1;
constexpr int DXT_TYPE_DXT5 = 2;

std::vector<uint8_t> decode_dxt(const uint8_t* data, int width, int height, int dxt_type);

} // namespace dxt
