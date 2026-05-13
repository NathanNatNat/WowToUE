/*!
	wow.export (https://github.com/Kruithne/wow.export)
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace db::caches::DBDecorCategories {

struct CategoryInfo {
	uint32_t id = 0;
	std::string name;
	uint32_t orderIndex = 0;
};

struct SubcategoryInfo {
	uint32_t id = 0;
	std::string name;
	uint32_t categoryID = 0;
	uint32_t orderIndex = 0;
};

/**
 * Initialize decor category data.
 */
void initialize_categories();

/**
 * Get all categories.
 */
const std::unordered_map<uint32_t, CategoryInfo>& get_all_categories();

/**
 * Get all subcategories.
 */
const std::unordered_map<uint32_t, SubcategoryInfo>& get_all_subcategories();

/**
 * Get subcategory IDs for a decor item.
 * @param decor_id Decor item ID.
 * @returns Pointer to set of subcategory IDs, or nullptr if none.
 */
const std::unordered_set<uint32_t>* get_subcategories_for_decor(uint32_t decor_id);

} // namespace db::caches::DBDecorCategories
