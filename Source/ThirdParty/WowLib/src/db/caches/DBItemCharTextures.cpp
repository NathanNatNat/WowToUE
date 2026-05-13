/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "DBItemCharTextures.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include "DBTextureFileData.h"
#include "DBComponentTextureFileData.h"

#include <format>
#include <unordered_map>
#include <map>

namespace db::caches::DBItemCharTextures {

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

// maps ItemID -> Map<ItemAppearanceModifierID, ItemDisplayInfoID>
static std::unordered_map<uint32_t, std::map<uint32_t, uint32_t>> item_to_display_ids;

// maps ItemDisplayInfoID -> array of { componentSection, materialResourcesID }
struct ComponentEntry {
	int section;
	uint32_t materialResourcesID;
};
static std::unordered_map<uint32_t, std::vector<ComponentEntry>> display_to_component_textures;

static bool is_initialized = false;

void initialize() {
	if (is_initialized)
		return;

	logging::write("Loading item character textures...");

	DBTextureFileData::ensureInitialized();
	DBComponentTextureFileData::initialize();

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

	for (const auto& [item_id, modifiers] : appearance_map) {
		for (const auto& [modifier_id, appearance_id] : modifiers) {
			auto it = appearance_to_display.find(appearance_id);
			if (it != appearance_to_display.end() && it->second != 0)
				item_to_display_ids[item_id][modifier_id] = it->second;
		}
	}

	// load component textures from ItemDisplayInfoMaterialRes
	for (const auto& [_id, row] : casc::db2::preloadTable("ItemDisplayInfoMaterialRes").getAllRows()) {
		(void)_id;
		uint32_t display_id = fieldToUint32(row.at("ItemDisplayInfoID"));
		ComponentEntry component;
		component.section = fieldToInt(row.at("ComponentSection"));
		component.materialResourcesID = fieldToUint32(row.at("MaterialResourcesID"));

		display_to_component_textures[display_id].push_back(component);
	}

	logging::write(std::format("Loaded character textures for {} item displays", display_to_component_textures.size()));
	is_initialized = true;
}

void ensureInitialized() {
	if (!is_initialized)
		initialize();
}

std::optional<std::vector<TextureComponent>> getTexturesByDisplayId(uint32_t display_id, int race_id, int gender_index) {
	auto comp_it = display_to_component_textures.find(display_id);
	if (comp_it == display_to_component_textures.end())
		return std::nullopt;

	std::vector<TextureComponent> result;
	for (const auto& component : comp_it->second) {
		const auto* file_data_ids = DBTextureFileData::getTextureFDIDsByMatID(component.materialResourcesID);
		if (file_data_ids && !file_data_ids->empty()) {
			std::optional<uint32_t> opt_race = (race_id >= 0) ? std::optional<uint32_t>(static_cast<uint32_t>(race_id)) : std::nullopt;
			std::optional<uint32_t> opt_gender = (gender_index >= 0) ? std::optional<uint32_t>(static_cast<uint32_t>(gender_index)) : std::nullopt;
			auto bestFileDataID = DBComponentTextureFileData::getTextureForRaceGender(
				*file_data_ids,
				opt_race,
				opt_gender,
				0);

			TextureComponent tc;
			tc.section = component.section;
			tc.fileDataID = bestFileDataID.value_or((*file_data_ids)[0]);
			result.push_back(tc);
		}
	}

	if (result.empty())
		return std::nullopt;
	return result;
}

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

	auto it0 = modifiers.find(0);
	if (it0 != modifiers.end())
		return it0->second;
	if (!modifiers.empty())
		return modifiers.begin()->second;
	return std::nullopt;
}

std::optional<std::vector<TextureComponent>> getItemTextures(uint32_t item_id, int race_id, int gender_index, int modifier_id) {
	auto disp_id = resolve_display_id(item_id, modifier_id);
	if (!disp_id)
		return std::nullopt;

	return getTexturesByDisplayId(*disp_id, race_id, gender_index);
}

std::optional<uint32_t> getDisplayId(uint32_t item_id, int modifier_id) {
	return resolve_display_id(item_id, modifier_id);
}

} // namespace db::caches::DBItemCharTextures
