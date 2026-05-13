/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBItemModels.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include "DBModelFileData.h"
#include "DBItemDisplayInfoModelMatRes.h"
#include "DBComponentModelFileData.h"

#include <format>
#include <map>
#include <unordered_map>
#include <algorithm>

namespace db::caches::DBItemModels {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::vector<uint32_t> fieldToUint32Vec(const db::FieldValue& val) {
	std::vector<uint32_t> result;
	if (auto* p = std::get_if<std::vector<int64_t>>(&val)) {
		result.reserve(p->size());
		for (auto v : *p)
			result.push_back(static_cast<uint32_t>(v));
	} else if (auto* p = std::get_if<std::vector<uint64_t>>(&val)) {
		result.reserve(p->size());
		for (auto v : *p)
			result.push_back(static_cast<uint32_t>(v));
	}
	return result;
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

// maps ItemID -> Map<modifier_id, ItemDisplayInfoID>
static std::unordered_map<uint32_t, std::map<uint32_t, uint32_t>> item_to_display_ids;

// maps ItemDisplayInfoID -> internal display data
struct InternalDisplayData {
	std::vector<std::vector<uint32_t>> modelOptions;
	std::vector<uint32_t> textures;
	std::vector<int> geosetGroup;
	std::vector<int> attachmentGeosetGroup;
};
static std::unordered_map<uint32_t, InternalDisplayData> display_to_data;

static bool is_initialized = false;

static ItemDisplayData resolveDisplayData(uint32_t display_id, const InternalDisplayData& data, int race_id, int gender_index, ShoulderPos shoulder_pos = ShoulderPos::None) {
	ItemDisplayData result;
	result.ID = display_id;
	result.textures = data.textures;
	result.geosetGroup = data.geosetGroup;
	result.attachmentGeosetGroup = data.attachmentGeosetGroup;

	// check if this is a shoulder-type item (2 model options with identical content)
	bool is_shoulder_style = false;
	if (data.modelOptions.size() == 2 &&
		!data.modelOptions[0].empty() &&
		!data.modelOptions[1].empty() &&
		data.modelOptions[0].size() == data.modelOptions[1].size()) {
		is_shoulder_style = std::equal(data.modelOptions[0].begin(), data.modelOptions[0].end(),
			data.modelOptions[1].begin());
	}

	if (is_shoulder_style && race_id >= 0 && gender_index >= 0) {
		// for shoulders, select models with different PositionIndex values
		const auto& options = data.modelOptions[0];
		auto candidates = DBComponentModelFileData::getModelsForRaceGenderByPosition(
			options, static_cast<uint32_t>(race_id), static_cast<uint32_t>(gender_index));

		if (shoulder_pos == ShoulderPos::Left) {
			if (candidates.left)
				result.models.push_back(*candidates.left);
		} else if (shoulder_pos == ShoulderPos::Right) {
			if (candidates.right)
				result.models.push_back(*candidates.right);
		} else {
			if (candidates.left)
				result.models.push_back(*candidates.left);
			if (candidates.right)
				result.models.push_back(*candidates.right);
		}
	} else {
		// standard logic for non-shoulder items
		for (const auto& options : data.modelOptions) {
			if (options.empty())
				continue;

			if (race_id >= 0 && gender_index >= 0) {
				auto best = DBComponentModelFileData::getModelForRaceGender(
					options, static_cast<uint32_t>(race_id), static_cast<uint32_t>(gender_index));
				if (best)
					result.models.push_back(*best);
			} else {
				result.models.push_back(options[0]);
			}
		}
	}

	return result;
}

void initialize() {
	if (is_initialized)
		return;

	logging::write("Loading item models...");

	DBModelFileData::initializeModelFileData();
	DBComponentModelFileData::initialize();
	DBItemDisplayInfoModelMatRes::ensureInitialized();

	// build item -> modifier -> appearance -> display chain
	// appearance_map: item_id -> Map<modifier_id, appearance_id>
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

	// load model and texture file data IDs from ItemDisplayInfo
	for (const auto& [display_id, row] : casc::db2::preloadTable("ItemDisplayInfo").getAllRows()) {
		auto allModelResIDs = fieldToUint32Vec(row.at("ModelResourcesID"));
		std::vector<uint32_t> model_res_ids;
		for (auto e : allModelResIDs) {
			if (e > 0)
				model_res_ids.push_back(e);
		}
		if (model_res_ids.empty())
			continue;

		// store ALL file data IDs per model resource (filter by race/gender at query time)
		std::vector<std::vector<uint32_t>> model_options;
		for (uint32_t model_res_id : model_res_ids) {
			const auto* file_data_ids = DBModelFileData::getModelFileDataID(model_res_id);
			if (file_data_ids && !file_data_ids->empty())
				model_options.push_back(*file_data_ids);
			else
				model_options.push_back({});
		}

		if (std::all_of(model_options.begin(), model_options.end(),
			[](const std::vector<uint32_t>& arr) { return arr.empty(); }))
			continue;

		// get texture file data IDs from display id
		std::vector<uint32_t> texture_file_data_ids;
		const auto* itemDisplayTexFileDataIDs = DBItemDisplayInfoModelMatRes::getItemDisplayIdTextureFileIds(display_id);
		if (itemDisplayTexFileDataIDs != nullptr)
			texture_file_data_ids.insert(texture_file_data_ids.end(), itemDisplayTexFileDataIDs->begin(), itemDisplayTexFileDataIDs->end());

		// geoset groups for character model and attachment/collection models
		auto geoGrpIt = row.find("GeosetGroup");
		auto attachGeoGrpIt = row.find("AttachmentGeosetGroup");

		InternalDisplayData data;
		data.modelOptions = std::move(model_options);
		data.textures = std::move(texture_file_data_ids);
		data.geosetGroup = (geoGrpIt != row.end()) ? fieldToIntVec(geoGrpIt->second) : std::vector<int>{};
		data.attachmentGeosetGroup = (attachGeoGrpIt != row.end()) ? fieldToIntVec(attachGeoGrpIt->second) : std::vector<int>{};

		display_to_data[display_id] = std::move(data);
	}

	logging::write(std::format("Loaded models for {} item displays", display_to_data.size()));
	is_initialized = true;
}

void ensureInitialized() {
	if (!is_initialized)
		initialize();
}

/**
 * Resolve display ID for an item with optional modifier.
 * Mirrors JS: resolve_display_id(item_id, modifier_id)
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
 * Get model file data IDs for an item (first option per model resource).
 */
std::optional<std::vector<uint32_t>> getItemModels(uint32_t item_id, int modifier_id) {
	auto disp_id = resolve_display_id(item_id, modifier_id);
	if (!disp_id)
		return std::nullopt;

	auto data_it = display_to_data.find(*disp_id);
	if (data_it == display_to_data.end())
		return std::nullopt;

	// return first option for each model resource, filtering falsy (0) values (JS filter(Boolean))
	std::vector<uint32_t> models;
	for (const auto& opts : data_it->second.modelOptions) {
		if (!opts.empty() && opts[0] != 0)
			models.push_back(opts[0]);
	}

	if (models.empty())
		return std::nullopt;
	return models;
}

/**
 * Get display data for an item (models and textures).
 */
std::optional<ItemDisplayData> getItemDisplay(uint32_t item_id, int race_id, int gender_index, int modifier_id, ShoulderPos shoulder_pos) {
	auto disp_id = resolve_display_id(item_id, modifier_id);
	if (!disp_id)
		return std::nullopt;

	auto data_it = display_to_data.find(*disp_id);
	if (data_it == display_to_data.end())
		return std::nullopt;

	return resolveDisplayData(*disp_id, data_it->second, race_id, gender_index, shoulder_pos);
}

/**
 * Get ItemDisplayInfoID for an item.
 */
std::optional<uint32_t> getDisplayId(uint32_t item_id, int modifier_id) {
	return resolve_display_id(item_id, modifier_id);
}

/**
 * Get available modifier IDs for an item.
 */
std::vector<uint32_t> getItemModifiers(uint32_t item_id) {
	auto mods_it = item_to_display_ids.find(item_id);
	if (mods_it == item_to_display_ids.end())
		return {};

	std::vector<uint32_t> result;
	result.reserve(mods_it->second.size());
	for (const auto& [modifier_id, _] : mods_it->second)
		result.push_back(modifier_id);
	return result;
}

/**
 * Get display data directly by ItemDisplayInfoID (skips item->display lookup).
 */
std::optional<ItemDisplayData> getDisplayData(uint32_t display_id, int race_id, int gender_index) {
	auto data_it = display_to_data.find(display_id);
	if (data_it == display_to_data.end())
		return std::nullopt;

	return resolveDisplayData(display_id, data_it->second, race_id, gender_index);
}

} // namespace db::caches::DBItemModels
