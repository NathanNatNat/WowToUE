/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace db::caches::DBItemModels {

struct ItemDisplayData {
	uint32_t ID = 0;
	std::vector<uint32_t> models;
	std::vector<uint32_t> textures;
	std::vector<int> geosetGroup;
	std::vector<int> attachmentGeosetGroup;
};

/**
 * Initialize item model data from DB2 tables.
 */
void initialize();

/**
 * Ensure item model data is initialized.
 */
void ensureInitialized();

/**
 * Get model file data IDs for an item (first option per model resource).
 * @param item_id Item ID.
 * @returns Vector of model file data IDs, or std::nullopt if not found.
 */
std::optional<std::vector<uint32_t>> getItemModels(uint32_t item_id, int modifier_id = -1);

// Shoulder position filter for getItemDisplay (matches JS 'left'/'right'/undefined)
enum class ShoulderPos { None = 0, Left = 1, Right = 2 };

/**
 * Get display data for an item (models and textures).
 * Filters models by race/gender if provided.
 * @param item_id Item ID.
 * @param race_id Character race ID for filtering (optional, -1 to skip).
 * @param gender_index 0=male, 1=female (optional, -1 to skip).
 * @param modifier_id Item appearance modifier ID (optional, -1 to use default).
 * @param shoulder_pos For shoulder items, return only Left or Right model (optional).
 * @returns ItemDisplayData, or std::nullopt if not found.
 */
std::optional<ItemDisplayData> getItemDisplay(uint32_t item_id, int race_id = -1, int gender_index = -1, int modifier_id = -1, ShoulderPos shoulder_pos = ShoulderPos::None);

/**
 * Get ItemDisplayInfoID for an item.
 * @param item_id Item ID.
 * @param modifier_id Item appearance modifier ID (optional, -1 to use default).
 * @returns Display ID, or std::nullopt if not found.
 */
std::optional<uint32_t> getDisplayId(uint32_t item_id, int modifier_id = -1);

/**
 * Get available modifier IDs for an item.
 * @param item_id Item ID.
 * @returns Sorted vector of modifier IDs.
 */
std::vector<uint32_t> getItemModifiers(uint32_t item_id);

/**
 * Get display data directly by ItemDisplayInfoID (skips item->display lookup).
 * @param display_id ItemDisplayInfoID.
 * @param race_id Character race ID for filtering (optional, -1 to skip).
 * @param gender_index 0=male, 1=female (optional, -1 to skip).
 * @returns ItemDisplayData, or std::nullopt if not found.
 */
std::optional<ItemDisplayData> getDisplayData(uint32_t display_id, int race_id = -1, int gender_index = -1);

} // namespace db::caches::DBItemModels
