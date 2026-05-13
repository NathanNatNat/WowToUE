/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>

class BufferWrapper;

/**
 * Process a null-terminated string block.
 *
 * @param data       Source buffer to read the chunk from.
 * @param chunkSize  Size of the string block in bytes.
 * @return           Map of byte-offset within the chunk to the null-terminated string
 *                   beginning at that offset.
 */
std::map<uint32_t, std::string> ReadStringBlock(BufferWrapper& data, uint32_t chunkSize);
