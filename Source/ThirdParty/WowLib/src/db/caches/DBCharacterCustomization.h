/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>

#include "../WDCReader.h"

namespace db::caches::DBCharacterCustomization {

struct OptionEntry {
	uint32_t id = 0;
	std::string label;
	bool is_color_swatch = false;
};

struct ChoiceEntry {
	uint32_t id = 0;
	std::string label;
	uint32_t swatch_color_0 = 0;
	uint32_t swatch_color_1 = 0;
};

struct ChrCustMaterialInfo {
	uint32_t ChrModelTextureTargetID = 0;
	std::optional<uint32_t> FileDataID;
};

struct ChrCustMaterialRef {
	uint32_t ChrCustomizationMaterialID = 0;
	uint32_t RelatedChrCustomizationChoiceID = 0;
};

struct ChrRaceInfo {
	uint32_t id = 0;
	std::string name;
	bool isNPCRace = false;
};

/**
 * Initialize character customization data from DB2 tables.
 */
void ensureInitialized();

// getters
std::optional<uint32_t> get_model_file_data_id(uint32_t model_id);
std::optional<uint32_t> get_texture_layout_id(uint32_t model_id);
const std::vector<OptionEntry>* get_options_for_model(uint32_t model_id);
const std::vector<ChoiceEntry>* get_choices_for_option(uint32_t option_id);
const std::vector<uint32_t>& get_default_options();
const std::map<uint32_t, std::vector<ChoiceEntry>>& get_option_to_choices_map();

std::optional<uint32_t> get_chr_model_id(uint32_t race_id, uint32_t sex);
const std::map<uint32_t, uint32_t>* get_race_models(uint32_t race_id);
const std::map<uint32_t, ChrRaceInfo>& get_chr_race_map();
const std::map<uint32_t, std::map<uint32_t, uint32_t>>& get_chr_race_x_chr_model_map();

std::optional<int> get_choice_geoset_id(uint32_t choice_id);
std::optional<uint32_t> get_choice_geoset_raw(uint32_t choice_id);
std::optional<int> get_geoset_value(uint32_t geoset_id);

const std::vector<ChrCustMaterialRef>* get_choice_materials(uint32_t choice_id);
const ChrCustMaterialInfo* get_chr_cust_material(uint32_t mat_id);

const db::DataRecord* get_model_texture_layer(uint32_t layout_id, uint32_t target_id);
const db::DataRecord* get_model_material(uint32_t layout_id, uint32_t texture_type);
const std::vector<db::DataRecord>* get_texture_sections(uint32_t layout_id);

const std::unordered_map<std::string, db::DataRecord>& get_model_material_map();
const std::unordered_map<std::string, db::DataRecord>& get_model_texture_layer_map();

std::optional<uint32_t> get_texture_file_data_id(uint32_t material_resources_id);

std::optional<uint32_t> get_choice_skinned_model(uint32_t choice_id);
const db::DataRecord* get_skinned_model(uint32_t id);

std::optional<uint32_t> get_choice_cond_model_file_data_id(uint32_t choice_id);

} // namespace db::caches::DBCharacterCustomization
