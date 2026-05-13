/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>

namespace db::caches::DBTextureFileData {

/**
 * Initialize texture file data ID from TextureFileData.db2
 */
void initializeTextureFileData();

/**
 * Ensure texture file data is initialized. Call this before using other functions.
 */
void ensureInitialized();

/**
 * Retrieves texture file data IDs by a material resource ID.
 * @param matResID Material resource ID.
 * @returns Pointer to vector of file data IDs, or nullptr if not found.
 */
const std::vector<uint32_t>* getTextureFDIDsByMatID(uint32_t matResID);

/**
 * Retrieve a list of all file data IDs cached from TextureFileData.db2
 * @returns Reference to the set of all texture file data IDs.
 */
const std::unordered_set<uint32_t>& getFileDataIDs();

} // namespace db::caches::DBTextureFileData
