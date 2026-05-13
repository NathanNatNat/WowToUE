/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace db::caches::DBItems {

struct ItemInfo {
	uint32_t id = 0;
	std::string name;
	uint32_t inventoryType = 0;
	uint32_t quality = 0;
	uint32_t classID = 0;
	uint32_t subclassID = 0;
};

/**
 * Initialize item cache from Item and ItemSparse DB2 tables.
 */
void initialize();

/**
 * Ensure item cache is initialized.
 */
void ensureInitialized();

/**
 * Get item info by item ID.
 * @returns Pointer to ItemInfo, or nullptr if not found.
 */
const ItemInfo* getItemById(uint32_t item_id);

/**
 * Get slot ID for an item.
 * @returns Slot ID, or std::nullopt if item not found or no mapping.
 */
std::optional<int> getItemSlotId(uint32_t item_id);

/**
 * Check if item cache is initialized.
 */
bool isInitialized();

/**
 * Check if an item is a bow (class=2, subclass=2).
 */
bool isItemBow(uint32_t item_id);

} // namespace db::caches::DBItems
