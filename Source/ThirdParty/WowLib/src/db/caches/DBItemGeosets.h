/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>

#include <nlohmann/json.hpp>

namespace db::caches::DBItemGeosets {

// geoset group enum (matches CharGeosets from WMV)
// the value * 100 gives the geoset base
namespace CG {
	constexpr int SKIN_OR_HAIR = 0;
	constexpr int FACE_1 = 1;
	constexpr int FACE_2 = 2;
	constexpr int FACE_3 = 3;
	constexpr int GLOVES = 4;
	constexpr int BOOTS = 5;
	constexpr int TAIL = 6;
	constexpr int EARS = 7;
	constexpr int SLEEVES = 8;
	constexpr int KNEEPADS = 9;
	constexpr int CHEST = 10;
	constexpr int PANTS = 11;
	constexpr int TABARD = 12;
	constexpr int TROUSERS = 13;
	constexpr int DH_LOINCLOTH = 14;
	constexpr int CLOAK = 15;
	constexpr int FACIAL_JEWELRY = 16;
	constexpr int EYEGLOW = 17;
	constexpr int BELT = 18;
	constexpr int BONE = 19;
	constexpr int FEET = 20;
	constexpr int SKULL = 21;
	constexpr int TORSO = 22;
	constexpr int HAND_ATTACHMENT = 23;
	constexpr int HEAD_ATTACHMENT = 24;
	constexpr int DH_BLINDFOLDS = 25;
	constexpr int SHOULDERS = 26;
	constexpr int HELM = 27;
	constexpr int ARM_UPPER = 28;
	constexpr int MECHAGNOME_ARMS = 29;
	constexpr int MECHAGNOME_LEGS = 30;
	constexpr int MECHAGNOME_FEET = 31;
	constexpr int HEAD_SWAP = 32;
	constexpr int EYES = 33;
	constexpr int EYEBROWS = 34;
	constexpr int PIERCINGS = 35;
	constexpr int NECKLACE = 36;
	constexpr int HEADDRESS = 37;
	constexpr int TAILS = 38;
	constexpr int MISC_ACCESSORY = 39;
	constexpr int MISC_FEATURE = 40;
	constexpr int NOSES = 41;
	constexpr int HAIR_DECO_A = 42;
	constexpr int HORN_DECO = 43;
	constexpr int BODY_SIZE = 44;
	constexpr int DRACTHYR = 46;
	constexpr int EYE_GLOW_B = 51;
} // namespace CG

struct SlotGeosetEntry {
	int group_index;
	int char_geoset;
	bool special_feet = false;
};

struct GeosetData {
	std::vector<int> geosetGroup;
	std::vector<int> helmetGeosetVis;
};

// slot id to geoset group mapping per WoWItem.cpp
extern const std::unordered_map<int, std::vector<SlotGeosetEntry>> SLOT_GEOSET_MAPPING;

// priority system for conflicting geoset groups
extern const std::unordered_map<int, std::vector<int>> GEOSET_PRIORITY;

/**
 * Initialize item geoset data from DB2 tables.
 */
void initialize();

/**
 * Ensure geoset data is initialized.
 */
void ensureInitialized();

/**
 * Get geoset data for an item's display.
 * @param item_id Item ID.
 * @param modifier_id Optional item appearance modifier (-1 for default).
 * @returns Pointer to GeosetData, or nullptr if not found.
 */
const GeosetData* getItemGeosetData(uint32_t item_id, int modifier_id = -1);

/**
 * Get ItemDisplayInfoID for an item.
 * @param item_id Item ID.
 * @returns Display ID, or std::nullopt if not found.
 */
std::optional<uint32_t> getDisplayId(uint32_t item_id, int modifier_id = -1);

/**
 * Calculate geoset visibility changes for equipped items.
 * @param equipped_items Map of slot_id -> item_id.
 * @param item_skins Optional JSON object mapping slot_id_string -> modifier_id.
 * @returns Map of char_geoset -> value.
 */
std::unordered_map<int, int> calculateEquipmentGeosets(const std::unordered_map<int, uint32_t>& equipped_items, const nlohmann::json& item_skins = nlohmann::json::object());

/**
 * Get the set of char_geosets affected by equipped items.
 * @param equipped_items Map of slot_id -> item_id.
 * @param item_skins Optional JSON object mapping slot_id_string -> modifier_id.
 * @returns Set of affected CG enum values.
 */
std::unordered_set<int> getAffectedCharGeosets(const std::unordered_map<int, uint32_t>& equipped_items, const nlohmann::json& item_skins = nlohmann::json::object());

/**
 * Get geoset groups that should be hidden when a helmet is equipped.
 * @param item_id Helmet item ID.
 * @param race_id Character's race ID.
 * @param gender_index 0 for male, 1 for female.
 * @param modifier_id Optional item appearance modifier ID (-1 for default).
 * @returns Vector of CG enum values to hide.
 */
std::vector<int> getHelmetHideGeosets(uint32_t item_id, uint32_t race_id, int gender_index, int modifier_id = -1);

/**
 * Get geoset data directly by ItemDisplayInfoID.
 * @param display_id ItemDisplayInfoID.
 * @returns Pointer to GeosetData, or nullptr.
 */
const GeosetData* getGeosetDataByDisplayId(uint32_t display_id);

/**
 * Calculate equipment geosets from a Map<slot_id, display_id> (display-ID-based).
 * @param slot_display_map Map of slot_id -> display_id.
 * @returns Map of char_geoset -> value.
 */
std::unordered_map<int, int> calculateEquipmentGeosetsByDisplay(const std::unordered_map<int, uint32_t>& slot_display_map);

/**
 * Get affected char geosets from a Map<slot_id, display_id> (display-ID-based).
 * @param slot_display_map Map of slot_id -> display_id.
 * @returns Set of affected CG enum values.
 */
std::unordered_set<int> getAffectedCharGeosetsByDisplay(const std::unordered_map<int, uint32_t>& slot_display_map);

/**
 * Get helmet hide geosets directly by display ID.
 * @param display_id ItemDisplayInfoID.
 * @param race_id Character's race ID.
 * @param gender_index 0 for male, 1 for female.
 * @returns Vector of CG enum values to hide.
 */
std::vector<int> getHelmetHideGeosetsByDisplayId(uint32_t display_id, uint32_t race_id, int gender_index);

} // namespace db::caches::DBItemGeosets
