/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "crc32.h"

#include <array>

static constexpr std::array<uint32_t, 256> makeTable() {
	std::array<uint32_t, 256> t{};
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t current = i;
		for (int j = 0; j < 8; j++) {
			if (current & 1)
				current = 0xEDB88320u ^ (current >> 1);
			else
				current = current >> 1;
		}
		t[i] = current;
	}
	return t;
}

static constexpr auto table = makeTable();

/**
 * Calculate the CRC32 value of a given buffer.
 * @param buf Span of bytes to compute CRC32 over.
 * @returns CRC32 checksum as uint32_t.
 */
uint32_t crc32(std::span<const uint8_t> buf) {
	uint32_t res = 0xFFFFFFFFu;
	for (std::size_t i = 0; i < buf.size(); i++)
		res = table[(res ^ buf[i]) & 0xFF] ^ (res >> 8);

	return res ^ 0xFFFFFFFFu;
}