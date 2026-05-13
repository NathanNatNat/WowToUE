/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <span>

/**
 * Calculate the CRC32 value of a given buffer.
 * @param buf Span of bytes to compute CRC32 over.
 * @returns CRC32 checksum as uint32_t.
 */
uint32_t crc32(std::span<const uint8_t> buf);
