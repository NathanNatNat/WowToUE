/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>

namespace db {

enum CompressionType : uint32_t {
	None                = 0,
	Bitpacked           = 1,
	CommonData          = 2,
	BitpackedIndexed    = 3,
	BitpackedIndexedArray = 4,
	BitpackedSigned     = 5
};

} // namespace db
