/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace db::caches::DBDecor {

struct DecorItem {
	uint32_t id = 0;
	std::string name;
	uint32_t modelFileDataID = 0;
	uint32_t thumbnailFileDataID = 0;
	uint32_t itemID = 0;
	uint32_t gameObjectID = 0;
	uint32_t type = 0;
	uint32_t modelType = 0;
};

/**
 * Initialize house decor data from HouseDecor DB2.
 */
void initializeDecorData();

/**
 * Get all decor items.
 * @returns Reference to the decor items map.
 */
const std::unordered_map<uint32_t, DecorItem>& getAllDecorItems();

/**
 * Get a decor item by ID.
 * @param id Decor item ID.
 * @returns Pointer to DecorItem, or nullptr if not found.
 */
const DecorItem* getDecorItemByID(uint32_t id);

/**
 * Get a decor item by model file data ID.
 * @param fileDataID Model file data ID.
 * @returns Pointer to DecorItem, or nullptr if not found.
 */
const DecorItem* getDecorItemByModelFileDataID(uint32_t fileDataID);

} // namespace db::caches::DBDecor
