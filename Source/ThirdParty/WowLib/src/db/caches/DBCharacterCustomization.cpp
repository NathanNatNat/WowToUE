/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBCharacterCustomization.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include "DBCreatures.h"

#include <format>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <map>

namespace db::caches::DBCharacterCustomization {

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

static std::string fieldToString(const db::FieldValue& val) {
	if (auto* p = std::get_if<std::string>(&val))
		return *p;
	return "";
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

static std::unordered_map<uint32_t, uint32_t> tfd_map;
static std::unordered_map<uint32_t, uint32_t> choice_to_geoset;
static std::unordered_map<uint32_t, std::vector<ChrCustMaterialRef>> choice_to_chr_cust_material_id;
static std::unordered_map<uint32_t, uint32_t> choice_to_skinned_model;
static std::unordered_map<uint32_t, uint32_t> choice_to_cond_file_data_id;
static std::vector<uint32_t> unsupported_choices;

static std::map<uint32_t, std::vector<OptionEntry>> options_by_chr_model;
static std::map<uint32_t, std::vector<ChoiceEntry>> option_to_choices;
static std::vector<uint32_t> default_options;

static std::unordered_map<uint32_t, std::optional<uint32_t>> chr_model_id_to_file_data_id;
static std::unordered_map<uint32_t, uint32_t> chr_model_id_to_texture_layout_id;

static std::map<uint32_t, ChrRaceInfo> chr_race_map_data;
static std::map<uint32_t, std::map<uint32_t, uint32_t>> chr_race_x_chr_model_map_data;

static std::unordered_map<std::string, db::DataRecord> chr_model_material_map_data;
static std::unordered_map<uint32_t, std::vector<db::DataRecord>> char_component_texture_section_map;
static std::unordered_map<std::string, db::DataRecord> chr_model_texture_layer_map_data;

static std::unordered_map<uint32_t, int> geoset_map;
static std::unordered_map<uint32_t, ChrCustMaterialInfo> chr_cust_mat_map;
static std::unordered_map<uint32_t, db::DataRecord> chr_cust_skinned_model_map;

static bool is_initialized = false;
static bool is_initializing = false;
static std::mutex init_mutex;
static std::condition_variable init_cv;

static void _initialize() {
	logging::write("Loading character customization data...");

	// texture file data mapping
	for (const auto& [_id, tfd_row] : casc::db2::preloadTable("TextureFileData").getAllRows()) {
		(void)_id;
		uint32_t usageType = fieldToUint32(tfd_row.at("UsageType"));
		if (usageType != 0)
			continue;
		uint32_t matResID = fieldToUint32(tfd_row.at("MaterialResourcesID"));
		tfd_map[matResID] = fieldToUint32(tfd_row.at("FileDataID"));
	}

	// creature data (needed for ChrModel -> FileDataID)
	DBCreatures::initializeCreatureData();

	// conditional model mapping (ChrCustomizationCondModel -> CreatureModelData -> FileDataID)
	std::unordered_map<uint32_t, uint32_t> cond_model_map;
	for (const auto& [cond_model_id, cond_model_row] : casc::db2::preloadTable("ChrCustomizationCondModel").getAllRows())
		cond_model_map[cond_model_id] = fieldToUint32(cond_model_row.at("CreatureModelDataID"));

	// customization elements
	for (const auto& [_id, elem_row] : casc::db2::preloadTable("ChrCustomizationElement").getAllRows()) {
		(void)_id;
		uint32_t choiceID = fieldToUint32(elem_row.at("ChrCustomizationChoiceID"));
		uint32_t geosetID = fieldToUint32(elem_row.at("ChrCustomizationGeosetID"));
		uint32_t skinnedModelID = fieldToUint32(elem_row.at("ChrCustomizationSkinnedModelID"));
		uint32_t boneSetID = fieldToUint32(elem_row.at("ChrCustomizationBoneSetID"));
		uint32_t condModelID = fieldToUint32(elem_row.at("ChrCustomizationCondModelID"));
		uint32_t displayInfoID = fieldToUint32(elem_row.at("ChrCustomizationDisplayInfoID"));
		uint32_t materialID = fieldToUint32(elem_row.at("ChrCustomizationMaterialID"));
		uint32_t relatedChoiceID = fieldToUint32(elem_row.at("RelatedChrCustomizationChoiceID"));

		if (geosetID != 0)
			choice_to_geoset[choiceID] = geosetID;

		if (skinnedModelID != 0) {
			choice_to_skinned_model[choiceID] = skinnedModelID;
			unsupported_choices.push_back(choiceID);
		}

		if (boneSetID != 0)
			unsupported_choices.push_back(choiceID);

		if (condModelID != 0) {
			auto cond_it = cond_model_map.find(condModelID);
			if (cond_it != cond_model_map.end()) {
				auto file_data_id = DBCreatures::getFileDataIDByModelDataID(cond_it->second);
				if (file_data_id.has_value())
					choice_to_cond_file_data_id[choiceID] = *file_data_id;
				else
					logging::write(std::format("ChrCustomizationCondModel {} references unknown CreatureModelData {}", condModelID, cond_it->second));
			}
		}

		if (displayInfoID != 0)
			unsupported_choices.push_back(choiceID);

		if (materialID != 0) {
			ChrCustMaterialRef ref;
			ref.ChrCustomizationMaterialID = materialID;
			ref.RelatedChrCustomizationChoiceID = relatedChoiceID;
			choice_to_chr_cust_material_id[choiceID].push_back(ref);

			auto mat_row_opt = casc::db2::preloadTable("ChrCustomizationMaterial").getRow(materialID);
			if (mat_row_opt.has_value()) {
				const auto& mat_row = mat_row_opt.value();
				ChrCustMaterialInfo mat_info;
				mat_info.ChrModelTextureTargetID = fieldToUint32(mat_row.at("ChrModelTextureTargetID"));
				uint32_t matResID = fieldToUint32(mat_row.at("MaterialResourcesID"));
				auto tfd_it = tfd_map.find(matResID);
				if (tfd_it != tfd_map.end())
					mat_info.FileDataID = tfd_it->second;
				uint32_t mat_row_id = fieldToUint32(mat_row.at("ID"));
				chr_cust_mat_map[mat_row_id] = mat_info;
			}
		}
	}

	// customization options + choices
	std::map<uint32_t, std::vector<std::pair<uint32_t, db::DataRecord>>> options_by_model_temp;
	std::map<uint32_t, std::vector<std::pair<uint32_t, db::DataRecord>>> choices_by_option_temp;

	for (const auto& [opt_id, opt_row] : casc::db2::preloadTable("ChrCustomizationOption").getAllRows())
		options_by_model_temp[fieldToUint32(opt_row.at("ChrModelID"))].push_back({opt_id, opt_row});

	for (const auto& [choice_id, choice_row] : casc::db2::preloadTable("ChrCustomizationChoice").getAllRows())
		choices_by_option_temp[fieldToUint32(choice_row.at("ChrCustomizationOptionID"))].push_back({choice_id, choice_row});

	// ChrModel -> FileDataID, texture layout, options, choices
	for (const auto& [chr_model_id, chr_model_row] : casc::db2::preloadTable("ChrModel").getAllRows()) {
		uint32_t displayID = fieldToUint32(chr_model_row.at("DisplayID"));
		std::optional<uint32_t> file_data_id = DBCreatures::getFileDataIDByDisplayID(displayID);

		chr_model_id_to_file_data_id[chr_model_id] = file_data_id;
		chr_model_id_to_texture_layout_id[chr_model_id] = fieldToUint32(chr_model_row.at("CharComponentTextureLayoutID"));

		auto model_opts_it = options_by_model_temp.find(chr_model_id);
		if (model_opts_it == options_by_model_temp.end())
			continue;

		for (const auto& [chr_customization_option_id, chr_customization_option_row] : model_opts_it->second) {
			uint32_t model_id = fieldToUint32(chr_customization_option_row.at("ChrModelID"));

			std::string option_name;
			std::string name_lang = fieldToString(chr_customization_option_row.at("Name_lang"));
			if (!name_lang.empty())
				option_name = name_lang;
			else
				option_name = std::format("Option {}", fieldToUint32(chr_customization_option_row.at("OrderIndex")));

			OptionEntry opt_entry;
			opt_entry.id = chr_customization_option_id;
			opt_entry.label = option_name;
			opt_entry.is_color_swatch = false;

			std::vector<ChoiceEntry> choice_list;

			auto opt_choices_it = choices_by_option_temp.find(chr_customization_option_id);
			if (opt_choices_it != choices_by_option_temp.end()) {
				for (const auto& [chr_customization_choice_id, chr_customization_choice_row] : opt_choices_it->second) {
					std::string choice_name;
					std::string choice_name_lang = fieldToString(chr_customization_choice_row.at("Name_lang"));
					if (!choice_name_lang.empty())
						choice_name = choice_name_lang;
					else
						choice_name = std::format("Choice {}", fieldToUint32(chr_customization_choice_row.at("OrderIndex")));

					auto swatch_colors = fieldToUint32Vec(chr_customization_choice_row.at("SwatchColor"));
					uint32_t swatch_color_0 = swatch_colors.size() > 0 ? swatch_colors[0] : 0;
					uint32_t swatch_color_1 = swatch_colors.size() > 1 ? swatch_colors[1] : 0;

					ChoiceEntry ce;
					ce.id = chr_customization_choice_id;
					ce.label = choice_name;
					ce.swatch_color_0 = swatch_color_0;
					ce.swatch_color_1 = swatch_color_1;
					choice_list.push_back(std::move(ce));
				}
			}

			bool is_color_swatch = false;
			for (const auto& c : choice_list) {
				if (c.swatch_color_0 != 0 || c.swatch_color_1 != 0) {
					is_color_swatch = true;
					break;
				}
			}

			opt_entry.is_color_swatch = is_color_swatch;
			option_to_choices[chr_customization_option_id] = std::move(choice_list);
			options_by_chr_model[model_id].push_back(std::move(opt_entry));

			uint32_t flags = fieldToUint32(chr_customization_option_row.at("Flags"));
			if (!(flags & 0x20))
				default_options.push_back(chr_customization_option_id);
		}
	}

	// races
	for (const auto& [chr_race_id, chr_race_row] : casc::db2::preloadTable("ChrRaces").getAllRows()) {
		uint32_t flags = fieldToUint32(chr_race_row.at("Flags"));
		ChrRaceInfo info;
		info.id = chr_race_id;
		info.name = fieldToString(chr_race_row.at("Name_lang"));
		info.isNPCRace = ((flags & 1) == 1 && chr_race_id != 23 && chr_race_id != 75);
		chr_race_map_data[chr_race_id] = std::move(info);
	}

	// race -> model mapping
	for (const auto& [_id, row] : casc::db2::preloadTable("ChrRaceXChrModel").getAllRows()) {
		(void)_id;
		uint32_t racesID = fieldToUint32(row.at("ChrRacesID"));
		uint32_t sex = fieldToUint32(row.at("Sex"));
		uint32_t modelID = fieldToUint32(row.at("ChrModelID"));
		chr_race_x_chr_model_map_data[racesID][sex] = modelID;
	}

	// model materials
	for (const auto& [_id, row] : casc::db2::preloadTable("ChrModelMaterial").getAllRows()) {
		(void)_id;
		uint32_t layoutsID = fieldToUint32(row.at("CharComponentTextureLayoutsID"));
		uint32_t texType = fieldToUint32(row.at("TextureType"));
		std::string key = std::format("{}-{}", layoutsID, texType);
		chr_model_material_map_data[key] = row;
	}

	// component texture sections
	for (const auto& [_id, row] : casc::db2::preloadTable("CharComponentTextureSections").getAllRows()) {
		(void)_id;
		uint32_t layoutID = fieldToUint32(row.at("CharComponentTextureLayoutID"));
		char_component_texture_section_map[layoutID].push_back(row);
	}

	// model texture layers
	for (const auto& [_id, row] : casc::db2::preloadTable("ChrModelTextureLayer").getAllRows()) {
		(void)_id;
		uint32_t layoutsID = fieldToUint32(row.at("CharComponentTextureLayoutsID"));
		auto targetVec = fieldToUint32Vec(row.at("ChrModelTextureTargetID"));
		uint32_t targetID = targetVec.empty() ? 0 : targetVec[0];
		std::string key = std::format("{}-{}", layoutsID, targetID);
		chr_model_texture_layer_map_data[key] = row;
	}

	// customization geosets
	for (const auto& [chr_customization_geoset_id, chr_customization_geoset_row] : casc::db2::preloadTable("ChrCustomizationGeoset").getAllRows()) {
		int geosetType = fieldToInt(chr_customization_geoset_row.at("GeosetType"));
		int geosetID = fieldToInt(chr_customization_geoset_row.at("GeosetID"));
		std::ostringstream oss;
		oss << std::setw(2) << std::setfill('0') << geosetType
		    << std::setw(2) << std::setfill('0') << geosetID;
		geoset_map[chr_customization_geoset_id] = std::stoi(oss.str());
	}

	// customization skinned models
	for (const auto& [chr_customization_skinned_model_id, chr_customization_skinned_model_row] : casc::db2::preloadTable("ChrCustomizationSkinnedModel").getAllRows())
		chr_cust_skinned_model_map[chr_customization_skinned_model_id] = chr_customization_skinned_model_row;

	logging::write("Character customization data loaded");
	{
		std::scoped_lock lock(init_mutex);
		is_initialized = true;
		is_initializing = false;
	}
	init_cv.notify_all();
}

void ensureInitialized() {
	{
		std::unique_lock lock(init_mutex);
		if (is_initialized)
			return;

		if (is_initializing) {
			init_cv.wait(lock, [] { return is_initialized || !is_initializing; });
			return;
		}

		is_initializing = true;
	}

	try {
		_initialize();
	} catch (...) {
		{
			std::scoped_lock lock(init_mutex);
			is_initializing = false;
		}
		init_cv.notify_all();
		throw;
	}
}

// getters
std::optional<uint32_t> get_model_file_data_id(uint32_t model_id) {
	auto it = chr_model_id_to_file_data_id.find(model_id);
	if (it != chr_model_id_to_file_data_id.end())
		return it->second;
	return std::nullopt;
}

std::optional<uint32_t> get_texture_layout_id(uint32_t model_id) {
	auto it = chr_model_id_to_texture_layout_id.find(model_id);
	if (it != chr_model_id_to_texture_layout_id.end())
		return it->second;
	return std::nullopt;
}

const std::vector<OptionEntry>* get_options_for_model(uint32_t model_id) {
	auto it = options_by_chr_model.find(model_id);
	if (it != options_by_chr_model.end())
		return &it->second;
	return nullptr;
}

const std::vector<ChoiceEntry>* get_choices_for_option(uint32_t option_id) {
	auto it = option_to_choices.find(option_id);
	if (it != option_to_choices.end())
		return &it->second;
	return nullptr;
}

const std::vector<uint32_t>& get_default_options() {
	return default_options;
}

const std::map<uint32_t, std::vector<ChoiceEntry>>& get_option_to_choices_map() {
	return option_to_choices;
}

std::optional<uint32_t> get_chr_model_id(uint32_t race_id, uint32_t sex) {
	auto models_it = chr_race_x_chr_model_map_data.find(race_id);
	if (models_it == chr_race_x_chr_model_map_data.end())
		return std::nullopt;

	auto sex_it = models_it->second.find(sex);
	if (sex_it != models_it->second.end())
		return sex_it->second;
	return std::nullopt;
}

const std::map<uint32_t, uint32_t>* get_race_models(uint32_t race_id) {
	auto it = chr_race_x_chr_model_map_data.find(race_id);
	if (it != chr_race_x_chr_model_map_data.end())
		return &it->second;
	return nullptr;
}

const std::map<uint32_t, ChrRaceInfo>& get_chr_race_map() {
	return chr_race_map_data;
}

const std::map<uint32_t, std::map<uint32_t, uint32_t>>& get_chr_race_x_chr_model_map() {
	return chr_race_x_chr_model_map_data;
}

std::optional<int> get_choice_geoset_id(uint32_t choice_id) {
	auto geo_it = choice_to_geoset.find(choice_id);
	if (geo_it == choice_to_geoset.end())
		return std::nullopt;

	auto val_it = geoset_map.find(geo_it->second);
	if (val_it != geoset_map.end())
		return val_it->second;
	return std::nullopt;
}

std::optional<uint32_t> get_choice_geoset_raw(uint32_t choice_id) {
	auto it = choice_to_geoset.find(choice_id);
	if (it != choice_to_geoset.end())
		return it->second;
	return std::nullopt;
}

std::optional<int> get_geoset_value(uint32_t geoset_id) {
	auto it = geoset_map.find(geoset_id);
	if (it != geoset_map.end())
		return it->second;
	return std::nullopt;
}

const std::vector<ChrCustMaterialRef>* get_choice_materials(uint32_t choice_id) {
	auto it = choice_to_chr_cust_material_id.find(choice_id);
	if (it != choice_to_chr_cust_material_id.end())
		return &it->second;
	return nullptr;
}

const ChrCustMaterialInfo* get_chr_cust_material(uint32_t mat_id) {
	auto it = chr_cust_mat_map.find(mat_id);
	if (it != chr_cust_mat_map.end())
		return &it->second;
	return nullptr;
}

const db::DataRecord* get_model_texture_layer(uint32_t layout_id, uint32_t target_id) {
	std::string key = std::format("{}-{}", layout_id, target_id);
	auto it = chr_model_texture_layer_map_data.find(key);
	if (it != chr_model_texture_layer_map_data.end())
		return &it->second;
	return nullptr;
}

const db::DataRecord* get_model_material(uint32_t layout_id, uint32_t texture_type) {
	std::string key = std::format("{}-{}", layout_id, texture_type);
	auto it = chr_model_material_map_data.find(key);
	if (it != chr_model_material_map_data.end())
		return &it->second;
	return nullptr;
}

const std::vector<db::DataRecord>* get_texture_sections(uint32_t layout_id) {
	auto it = char_component_texture_section_map.find(layout_id);
	if (it != char_component_texture_section_map.end())
		return &it->second;
	return nullptr;
}

const std::unordered_map<std::string, db::DataRecord>& get_model_material_map() {
	return chr_model_material_map_data;
}

const std::unordered_map<std::string, db::DataRecord>& get_model_texture_layer_map() {
	return chr_model_texture_layer_map_data;
}

std::optional<uint32_t> get_texture_file_data_id(uint32_t material_resources_id) {
	auto it = tfd_map.find(material_resources_id);
	if (it != tfd_map.end())
		return it->second;
	return std::nullopt;
}

std::optional<uint32_t> get_choice_skinned_model(uint32_t choice_id) {
	auto it = choice_to_skinned_model.find(choice_id);
	if (it != choice_to_skinned_model.end())
		return it->second;
	return std::nullopt;
}

const db::DataRecord* get_skinned_model(uint32_t id) {
	auto it = chr_cust_skinned_model_map.find(id);
	if (it != chr_cust_skinned_model_map.end())
		return &it->second;
	return nullptr;
}

std::optional<uint32_t> get_choice_cond_model_file_data_id(uint32_t choice_id) {
	auto it = choice_to_cond_file_data_id.find(choice_id);
	if (it != choice_to_cond_file_data_id.end())
		return it->second;
	return std::nullopt;
}

} // namespace db::caches::DBCharacterCustomization
