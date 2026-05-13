/*!
	wow.export (https://github.com/Kruithne/wow.export)
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>

namespace db::caches::DBItemDisplayInfoModelMatRes {

/**
 * Initialize Item Display Info Model Mat Res DB2.
 */
void initialize();

/**
 * Ensure initialized.
 */
void ensureInitialized();

/**
 * Get texture file data IDs by ItemDisplayInfoID.
 * @param ItemDisplayInfoId ItemDisplayInfoID.
 * @returns Pointer to vector of texture file data IDs, or nullptr if not found.
 */
const std::vector<uint32_t>* getItemDisplayIdTextureFileIds(uint32_t ItemDisplayInfoId);

} // namespace db::caches::DBItemDisplayInfoModelMatRes
