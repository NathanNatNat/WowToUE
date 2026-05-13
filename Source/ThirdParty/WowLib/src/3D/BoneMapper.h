/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>

/**
 * Get the label for a bone.
 * @param bone_id  - The bone ID.
 * @param index - The bone index.
 * @param crc - CRC for fallback
 * @returns The bone label.
 */
std::string get_bone_name(int bone_id, int index, uint32_t crc);
