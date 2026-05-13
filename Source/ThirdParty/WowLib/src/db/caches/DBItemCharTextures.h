/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace db::caches::DBItemCharTextures {

// component section enum (matches CharComponentTextureSections.SectionType)
namespace COMPONENT_SECTION {
	constexpr int ARM_UPPER = 0;
	constexpr int ARM_LOWER = 1;
	constexpr int HAND = 2;
	constexpr int TORSO_UPPER = 3;
	constexpr int TORSO_LOWER = 4;
	constexpr int LEG_UPPER = 5;
	constexpr int LEG_LOWER = 6;
	constexpr int FOOT = 7;
	constexpr int ACCESSORY = 8;
} // namespace COMPONENT_SECTION

struct TextureComponent {
	int section = 0;
	uint32_t fileDataID = 0;
};

/**
 * Initialize item character textures from DB2 tables.
 */
void initialize();

/**
 * Ensure item character textures are initialized.
 */
void ensureInitialized();

/**
 * Get character texture components for an item.
 * @param item_id Item ID.
 * @param race_id Character race ID for filtering (optional, -1 to skip).
 * @param gender_index 0=male, 1=female (optional, -1 to skip).
 * @param modifier_id Item appearance modifier ID (optional, -1 for default).
 * @returns Vector of TextureComponent, or nullptr if not found.
 */
std::optional<std::vector<TextureComponent>> getItemTextures(uint32_t item_id, int race_id = -1, int gender_index = -1, int modifier_id = -1);

/**
 * Get ItemDisplayInfoID for an item.
 * @param item_id Item ID.
 * @param modifier_id Item appearance modifier ID (optional, -1 for default).
 * @returns Display ID, or std::nullopt if not found.
 */
std::optional<uint32_t> getDisplayId(uint32_t item_id, int modifier_id = -1);

/**
 * Get character texture components directly by ItemDisplayInfoID.
 * @param display_id ItemDisplayInfoID.
 * @param race_id Character race ID for filtering (optional, -1 to skip).
 * @param gender_index 0=male, 1=female (optional, -1 to skip).
 * @returns Vector of TextureComponent, or std::nullopt if not found.
 */
std::optional<std::vector<TextureComponent>> getTexturesByDisplayId(uint32_t display_id, int race_id = -1, int gender_index = -1);

} // namespace db::caches::DBItemCharTextures
