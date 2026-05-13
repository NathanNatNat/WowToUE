/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>

namespace db::caches::DBModelFileData {

/**
 * Initialize model file data from ModelFileData.db2
 */
void initializeModelFileData();

/**
 * Retrieve model file data IDs for a given model resource ID.
 * @param modelResID Model resource ID to look up.
 * @returns Pointer to vector of file data IDs, or nullptr if not found.
 */
const std::vector<uint32_t>* getModelFileDataID(uint32_t modelResID);

/**
 * Retrieve the set of all file data IDs cached from ModelFileData.db2
 * @returns Reference to the set of all model file data IDs.
 */
const std::unordered_set<uint32_t>& getFileDataIDs();

} // namespace db::caches::DBModelFileData
