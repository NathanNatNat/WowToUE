/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBItemGeosets.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <algorithm>
#include <map>

namespace db::caches::DBItemGeosets {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static int fieldToInt(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<int>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<int>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<int>(*p);
	return 0;
}

static std::vector<int> fieldToIntVec(const db::FieldValue& val) {
	std::vector<int> result;
	if (auto* p = std::get_if<std::vector<int64_t>>(&val)) {
		result.reserve(p->size());
		for (auto v : *p)
			result.push_back(static_cast<int>(v));
	} else if (auto* p = std::get_if<std::vector<uint64_t>>(&val)) {
		result.reserve(p->size());
		for (auto v : *p)
			result.push_back(static_cast<int>(v));
	}
	return result;
}

// slot id to geoset group mapping per WoWItem.cpp
const std::unordered_map<int, std::vector<SlotGeosetEntry>> SLOT_GEOSET_MAPPING = {
	// Head: geosetGroup[0] = HELM, geosetGroup[1] = SKULL
	{1, {
		{0, CG::HELM, false},
		{1, CG::SKULL, false}
	}},
	// Shoulder (L): geosetGroup[0] = SHOULDERS
	{3, {
		{0, CG::SHOULDERS, false}
	}},
	// Shoulder (R): same geoset group as left
	{30, {
		{0, CG::SHOULDERS, false}
	}},
	// Shirt: geosetGroup[0] = CG_SLEEVES, geosetGroup[1] = CG_CHEST
	{4, {
		{0, CG::SLEEVES, false},
		{1, CG::CHEST, false}
	}},
	// Chest: geosetGroup[0] = CG_SLEEVES, geosetGroup[1] = CG_CHEST, geosetGroup[2] = CG_TROUSERS, geosetGroup[3] = CG_TORSO, geosetGroup[4] = ARM_UPPER
	{5, {
		{0, CG::SLEEVES, false},
		{1, CG::CHEST, false},
		{2, CG::TROUSERS, false},
		{3, CG::TORSO, false},
		{4, CG::ARM_UPPER, false}
	}},
	// Waist/Belt: geosetGroup[0] = CG_BELT
	{6, {
		{0, CG::BELT, false}
	}},
	// Pants/Legs: geosetGroup[0] = CG_PANTS, geosetGroup[1] = CG_KNEEPADS, geosetGroup[2] = CG_TROUSERS
	{7, {
		{0, CG::PANTS, false},
		{1, CG::KNEEPADS, false},
		{2, CG::TROUSERS, false}
	}},
	// Boots/Feet: geosetGroup[0] = CG_BOOTS, geosetGroup[1] = CG_FEET (special handling)
	{8, {
		{0, CG::BOOTS, false},
		{1, CG::FEET, true}
	}},
	// Wrist/Bracers: no geoset groups
	{9, {}},
	// Hands/Gloves: geosetGroup[0] = CG_GLOVES, geosetGroup[1] = CG_HAND_ATTACHMENT
	{10, {
		{0, CG::GLOVES, false},
		{1, CG::HAND_ATTACHMENT, false}
	}},
	// Back/Cloak: geosetGroup[0] = CG_CLOAK
	{15, {
		{0, CG::CLOAK, false}
	}},
	// Tabard: geosetGroup[0] = CG_TABARD
	{19, {
		{0, CG::TABARD, false}
	}}
};

// priority system for conflicting geoset groups
const std::unordered_map<int, std::vector<int>> GEOSET_PRIORITY = {
	{CG::SLEEVES, {10, 5, 4}},      // gloves > chest > shirt
	{CG::CHEST, {5, 4}},            // chest > shirt
	{CG::TROUSERS, {5, 7}},         // chest > pants
	{CG::TABARD, {19}},             // tabard only
	{CG::CLOAK, {15}},              // cloak only
	{CG::BELT, {6}},                // belt only
	{CG::FEET, {8}},                // boots only
	{CG::TORSO, {5}},               // chest only
	{CG::HAND_ATTACHMENT, {10}},    // gloves only
	{CG::HELM, {1}},                // head only
	{CG::ARM_UPPER, {5}},           // chest only
	{CG::SKULL, {1}},               // head only
	{CG::SHOULDERS, {3, 30}},       // shoulder (L or R)
	{CG::BOOTS, {8}},               // boots only
	{CG::GLOVES, {10}},             // gloves only
	{CG::PANTS, {7}},               // pants only
	{CG::KNEEPADS, {7}}             // pants only
};

// maps ItemID -> Map<modifier_id, ItemDisplayInfoID>
static std::unordered_map<uint32_t, std::map<uint32_t, uint32_t>> item_to_display_ids;

// maps ItemDisplayInfoID -> geoset data
static std::unordered_map<uint32_t, GeosetData> display_to_geosets;

// maps HelmetGeosetVisDataID -> Map<RaceID, vector<int>>
static std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::vector<int>>> helmet_hide_map;

static bool is_initialized = false;

void initialize() {
	if (is_initialized)
		return;

	logging::write("Loading item geosets...");

	// build item -> modifier -> appearance -> display chain
	std::unordered_map<uint32_t, std::map<uint32_t, uint32_t>> appearance_map;
	for (const auto& [_id, row] : casc::db2::preloadTable("ItemModifiedAppearance").getAllRows()) {
		(void)_id;
		uint32_t itemID = fieldToUint32(row.at("ItemID"));
		uint32_t appearanceID = fieldToUint32(row.at("ItemAppearanceID"));
		uint32_t modifierID = 0;
		auto mod_it = row.find("ItemAppearanceModifierID");
		if (mod_it != row.end())
			modifierID = fieldToUint32(mod_it->second);
		appearance_map[itemID][modifierID] = appearanceID;
	}

	std::unordered_map<uint32_t, uint32_t> appearance_to_display;
	for (const auto& [id, row] : casc::db2::preloadTable("ItemAppearance").getAllRows()) {
		uint32_t displayID = fieldToUint32(row.at("ItemDisplayInfoID"));
		appearance_to_display[id] = displayID;
	}

	// map item id -> modifier_id -> display id
	for (const auto& [item_id, modifiers] : appearance_map) {
		for (const auto& [modifier_id, appearance_id] : modifiers) {
			auto it = appearance_to_display.find(appearance_id);
			if (it != appearance_to_display.end() && it->second != 0)
				item_to_display_ids[item_id][modifier_id] = it->second;
		}
	}

	// load geoset groups from ItemDisplayInfo
	for (const auto& [display_id, row] : casc::db2::preloadTable("ItemDisplayInfo").getAllRows()) {
		auto geoGrpIt = row.find("GeosetGroup");
		auto helmetVisIt = row.find("HelmetGeosetVis");

		std::vector<int> geoset_group;
		std::vector<int> helmet_geoset_vis;

		if (geoGrpIt != row.end())
			geoset_group = fieldToIntVec(geoGrpIt->second);

		if (helmetVisIt != row.end())
			helmet_geoset_vis = fieldToIntVec(helmetVisIt->second);

		bool has_geoset = geoGrpIt != row.end();
		bool has_helmet = helmetVisIt != row.end();

		if (has_geoset || has_helmet) {
			GeosetData data;
			data.geosetGroup = geoset_group.empty() ? std::vector<int>{0, 0, 0, 0, 0, 0} : std::move(geoset_group);
			data.helmetGeosetVis = helmet_geoset_vis.empty() ? std::vector<int>{0, 0} : std::move(helmet_geoset_vis);
			while (data.geosetGroup.size() < 6)
				data.geosetGroup.push_back(0);
			while (data.helmetGeosetVis.size() < 2)
				data.helmetGeosetVis.push_back(0);
			display_to_geosets[display_id] = std::move(data);
		}
	}

	// load helmet hide data from HelmetGeosetData
	for (const auto& [_id, row] : casc::db2::preloadTable("HelmetGeosetData").getAllRows()) {
		(void)_id;
		uint32_t vis_id = fieldToUint32(row.at("HelmetGeosetVisDataID"));
		uint32_t race_id = fieldToUint32(row.at("RaceID"));
		int hide_group = fieldToInt(row.at("HideGeosetGroup"));

		helmet_hide_map[vis_id][race_id].push_back(hide_group);
	}

	logging::write(std::format("Loaded geosets for {} item displays, {} helmet visibility rules", display_to_geosets.size(), helmet_hide_map.size()));
	is_initialized = true;
}

void ensureInitialized() {
	if (!is_initialized)
		initialize();
}

/**
 * Resolve display ID for an item with optional modifier.
 * Prefers modifier 0, then lowest available modifier.
 */
static std::optional<uint32_t> resolve_display_id(uint32_t item_id, int modifier_id = -1) {
	auto mods_it = item_to_display_ids.find(item_id);
	if (mods_it == item_to_display_ids.end())
		return std::nullopt;

	const auto& modifiers = mods_it->second;
	if (modifier_id >= 0) {
		auto it = modifiers.find(static_cast<uint32_t>(modifier_id));
		if (it != modifiers.end())
			return it->second;
		return std::nullopt;
	}

	// default: prefer modifier 0, else lowest available
	auto it0 = modifiers.find(0);
	if (it0 != modifiers.end())
		return it0->second;
	if (!modifiers.empty())
		return modifiers.begin()->second;
	return std::nullopt;
}

/**
 * Get geoset data for an item's display (with optional modifier).
 */
static const GeosetData* getItemGeosetDataWithModifier(uint32_t item_id, int modifier_id) {
	auto disp_id = resolve_display_id(item_id, modifier_id);
	if (!disp_id)
		return nullptr;

	auto geo_it = display_to_geosets.find(*disp_id);
	if (geo_it != display_to_geosets.end())
		return &geo_it->second;
	return nullptr;
}

/**
 * Get geoset data for an item's display (with optional modifier).
 */
const GeosetData* getItemGeosetData(uint32_t item_id, int modifier_id) {
	return getItemGeosetDataWithModifier(item_id, modifier_id);
}

/**
 * Get ItemDisplayInfoID for an item.
 */
std::optional<uint32_t> getDisplayId(uint32_t item_id, int modifier_id) {
	return resolve_display_id(item_id, modifier_id);
}

/**
 * Calculate geoset visibility changes for equipped items.
 */
std::unordered_map<int, int> calculateEquipmentGeosets(const std::unordered_map<int, uint32_t>& equipped_items, const nlohmann::json& item_skins) {
	// track values per char_geoset, grouped by slot for priority resolution
	struct SlotValue {
		int slot_id;
		int value;
	};
	std::unordered_map<int, std::vector<SlotValue>> char_geoset_to_slot_values;

	for (const auto& [slot_id, item_id] : equipped_items) {
		auto mapping_it = SLOT_GEOSET_MAPPING.find(slot_id);
		if (mapping_it == SLOT_GEOSET_MAPPING.end())
			continue;

		int modifier_id = -1;
		std::string slot_str = std::to_string(slot_id);
		if (item_skins.is_object() && item_skins.contains(slot_str))
			modifier_id = item_skins[slot_str].get<int>();

		const GeosetData* geoset_data = getItemGeosetDataWithModifier(item_id, modifier_id);
		if (!geoset_data)
			continue;

		for (const auto& entry : mapping_it->second) {
			int group_value = (entry.group_index < static_cast<int>(geoset_data->geosetGroup.size()))
				? geoset_data->geosetGroup[entry.group_index] : 0;

			// calculate the value to use (WMV uses 1 + geosetGroup[n] for most)
			int value;
			if (entry.special_feet) {
				// CG_FEET special handling per WoWItem.cpp:
				// if geosetGroup[1] == 0, use 2
				// if geosetGroup[1] > 0, use geosetGroup[1]
				value = group_value == 0 ? 2 : group_value;
			} else {
				value = 1 + group_value;
			}

			char_geoset_to_slot_values[entry.char_geoset].push_back({slot_id, value});
		}
	}

	// resolve priorities and build final result
	std::unordered_map<int, int> result;

	for (const auto& [char_geoset, slot_values] : char_geoset_to_slot_values) {
		auto priority_it = GEOSET_PRIORITY.find(char_geoset);

		if (priority_it != GEOSET_PRIORITY.end()) {
			for (int priority_slot : priority_it->second) {
				auto entry_it = std::find_if(slot_values.begin(), slot_values.end(),
					[priority_slot](const SlotValue& sv) { return sv.slot_id == priority_slot; });
				if (entry_it != slot_values.end()) {
					result[char_geoset] = entry_it->value;
					break;
				}
			}
		} else if (!slot_values.empty()) {
			result[char_geoset] = slot_values[0].value;
		}
	}

	return result;
}

/**
 * Get geoset groups that should be hidden when a helmet is equipped.
 */
std::vector<int> getHelmetHideGeosets(uint32_t item_id, uint32_t race_id, int gender_index, int modifier_id) {
	const GeosetData* geoset_data = getItemGeosetDataWithModifier(item_id, modifier_id);
	if (!geoset_data || geoset_data->helmetGeosetVis.empty())
		return {};

	if (gender_index < 0 || gender_index >= static_cast<int>(geoset_data->helmetGeosetVis.size()))
		return {};

	int vis_id = geoset_data->helmetGeosetVis[gender_index];
	if (vis_id == 0)
		return {};

	auto vis_it = helmet_hide_map.find(static_cast<uint32_t>(vis_id));
	if (vis_it == helmet_hide_map.end())
		return {};

	auto race_it = vis_it->second.find(race_id);
	if (race_it != vis_it->second.end())
		return race_it->second;
	return {};
}

/**
 * Get the set of char_geosets affected by equipped items.
 */
std::unordered_set<int> getAffectedCharGeosets(const std::unordered_map<int, uint32_t>& equipped_items, const nlohmann::json& item_skins) {
	std::unordered_set<int> affected;

	for (const auto& [slot_id, item_id] : equipped_items) {
		auto mapping_it = SLOT_GEOSET_MAPPING.find(slot_id);
		if (mapping_it == SLOT_GEOSET_MAPPING.end())
			continue;

		int modifier_id = -1;
		std::string slot_str = std::to_string(slot_id);
		if (item_skins.is_object() && item_skins.contains(slot_str))
			modifier_id = item_skins[slot_str].get<int>();

		// skip items without display data (hidden items)
		const GeosetData* geoset_data = getItemGeosetDataWithModifier(item_id, modifier_id);
		if (!geoset_data)
			continue;

		for (const auto& entry : mapping_it->second)
			affected.insert(entry.char_geoset);
	}

	return affected;
}

/**
 * Get geoset data directly by ItemDisplayInfoID.
 */
const GeosetData* getGeosetDataByDisplayId(uint32_t display_id) {
	auto it = display_to_geosets.find(display_id);
	if (it != display_to_geosets.end())
		return &it->second;
	return nullptr;
}

/**
 * Calculate equipment geosets from a Map<slot_id, display_id> (display-ID-based).
 */
std::unordered_map<int, int> calculateEquipmentGeosetsByDisplay(const std::unordered_map<int, uint32_t>& slot_display_map) {
	struct SlotValue {
		int slot_id;
		int value;
	};
	std::unordered_map<int, std::vector<SlotValue>> char_geoset_to_slot_values;

	for (const auto& [slot_id, display_id] : slot_display_map) {
		auto mapping_it = SLOT_GEOSET_MAPPING.find(slot_id);
		if (mapping_it == SLOT_GEOSET_MAPPING.end())
			continue;

		auto geo_it = display_to_geosets.find(display_id);
		if (geo_it == display_to_geosets.end())
			continue;

		const GeosetData& geoset_data = geo_it->second;

		for (const auto& entry : mapping_it->second) {
			int group_value = (entry.group_index < static_cast<int>(geoset_data.geosetGroup.size()))
				? geoset_data.geosetGroup[entry.group_index] : 0;

			int value;
			if (entry.special_feet)
				value = group_value == 0 ? 2 : group_value;
			else
				value = 1 + group_value;

			char_geoset_to_slot_values[entry.char_geoset].push_back({slot_id, value});
		}
	}

	std::unordered_map<int, int> result;

	for (const auto& [char_geoset, slot_values] : char_geoset_to_slot_values) {
		auto priority_it = GEOSET_PRIORITY.find(char_geoset);

		if (priority_it != GEOSET_PRIORITY.end()) {
			for (int priority_slot : priority_it->second) {
				auto entry_it = std::find_if(slot_values.begin(), slot_values.end(),
					[priority_slot](const SlotValue& sv) { return sv.slot_id == priority_slot; });
				if (entry_it != slot_values.end()) {
					result[char_geoset] = entry_it->value;
					break;
				}
			}
		} else if (!slot_values.empty()) {
			result[char_geoset] = slot_values[0].value;
		}
	}

	return result;
}

/**
 * Get affected char geosets from a Map<slot_id, display_id> (display-ID-based).
 */
std::unordered_set<int> getAffectedCharGeosetsByDisplay(const std::unordered_map<int, uint32_t>& slot_display_map) {
	std::unordered_set<int> affected;

	for (const auto& [slot_id, display_id] : slot_display_map) {
		auto mapping_it = SLOT_GEOSET_MAPPING.find(slot_id);
		if (mapping_it == SLOT_GEOSET_MAPPING.end())
			continue;

		auto geo_it = display_to_geosets.find(display_id);
		if (geo_it == display_to_geosets.end())
			continue;

		for (const auto& entry : mapping_it->second)
			affected.insert(entry.char_geoset);
	}

	return affected;
}

/**
 * Get helmet hide geosets directly by display ID.
 */
std::vector<int> getHelmetHideGeosetsByDisplayId(uint32_t display_id, uint32_t race_id, int gender_index) {
	auto geo_it = display_to_geosets.find(display_id);
	if (geo_it == display_to_geosets.end() || geo_it->second.helmetGeosetVis.empty())
		return {};

	if (gender_index < 0 || gender_index >= static_cast<int>(geo_it->second.helmetGeosetVis.size()))
		return {};

	int vis_id = geo_it->second.helmetGeosetVis[gender_index];
	if (vis_id == 0)
		return {};

	auto vis_it = helmet_hide_map.find(static_cast<uint32_t>(vis_id));
	if (vis_it == helmet_hide_map.end())
		return {};

	auto race_it = vis_it->second.find(race_id);
	if (race_it != vis_it->second.end())
		return race_it->second;
	return {};
}

} // namespace db::caches::DBItemGeosets
