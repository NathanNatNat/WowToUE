/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
*/

#include "DBCreaturesLegacy.h"

#include "../../log.h"
#include "../../buffer.h"
#include "../DBCReader.h"

#include <format>
#include <algorithm>
#include <regex>
#include <filesystem>

namespace db::caches::DBCreaturesLegacy {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::string fieldToString(const db::FieldValue& val) {
	if (auto* p = std::get_if<std::string>(&val))
		return *p;
	return "";
}

static std::vector<std::string> fieldToStringVec(const db::FieldValue& val) {
	if (auto* p = std::get_if<std::vector<std::string>>(&val))
		return *p;
	return {};
}

static std::unordered_map<std::string, std::vector<LegacyCreatureDisplay>> creatureDisplays;
static bool isInitialized = false;

static std::string normalizePath(const std::string& path) {
	std::string normalized = path;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		[](unsigned char c) { return std::tolower(c); });
	std::replace(normalized.begin(), normalized.end(), '\\', '/');

	if (normalized.size() >= 4) {
		std::string ext = normalized.substr(normalized.size() - 4);
		if (ext == ".mdl" || ext == ".mdx")
			normalized = normalized.substr(0, normalized.size() - 4) + ".m2";
	}

	return normalized;
}

static std::string getDirectory(const std::string& path) {
	auto pos = path.find_last_of('/');
	if (pos != std::string::npos)
		return path.substr(0, pos);
	return "";
}

void initializeCreatureData(std::function<std::vector<uint8_t>(const std::string&)> getFile, const std::string& build_id) {
	if (isInitialized)
		return;

	logging::write("Loading legacy creature textures from DBC...");

	try {
		// load CreatureModelData.dbc
		auto model_data_raw = getFile("DBFilesClient\\CreatureModelData.dbc");
		if (model_data_raw.empty()) {
			logging::write("CreatureModelData.dbc not found in MPQ");
			return;
		}

		db::DBCReader model_data_reader("CreatureModelData.dbc", build_id);
		BufferWrapper model_buf = BufferWrapper::from(model_data_raw);
		model_data_reader.parse(model_buf);

		// build map of modelID -> model filepath
		std::unordered_map<uint32_t, std::string> model_id_to_path;

		auto model_rows = model_data_reader.getAllRows();
		for (const auto& [id, row] : model_rows) {
			// CreatureModelData has ModelPath field (string)
			std::string model_path;
			auto it = row.find("ModelName");
			if (it != row.end())
				model_path = fieldToString(it->second);
			if (model_path.empty()) {
				it = row.find("ModelPath");
				if (it != row.end())
					model_path = fieldToString(it->second);
			}
			if (model_path.empty()) {
				it = row.find("field_2");
				if (it != row.end())
					model_path = fieldToString(it->second);
			}

			if (!model_path.empty()) {
				// normalize: lowercase, convert .mdl/.mdx to .m2
				std::string normalized = normalizePath(model_path);
				model_id_to_path.emplace(id, std::move(normalized));
			}
		}

		logging::write(std::format("Loaded {} creature models from CreatureModelData.dbc", model_id_to_path.size()));

		// load CreatureDisplayInfo.dbc
		auto display_info_raw = getFile("DBFilesClient\\CreatureDisplayInfo.dbc");
		if (display_info_raw.empty()) {
			logging::write("CreatureDisplayInfo.dbc not found in MPQ");
			return;
		}

		db::DBCReader display_info_reader("CreatureDisplayInfo.dbc", build_id);
		BufferWrapper display_buf = BufferWrapper::from(display_info_raw);
		display_info_reader.parse(display_buf);

		auto display_rows = display_info_reader.getAllRows();

		for (const auto& [display_id, row] : display_rows) {
			uint32_t model_id = 0;
			bool model_id_found = false;
			auto mit = row.find("ModelID");
			if (mit != row.end()) {
				model_id = fieldToUint32(mit->second);
				model_id_found = true;
			}
			if (!model_id_found) {
				mit = row.find("field_1");
				if (mit != row.end())
					model_id = fieldToUint32(mit->second);
			}

			auto pathIt = model_id_to_path.find(model_id);
			if (pathIt == model_id_to_path.end())
				continue;

			const std::string& model_path = pathIt->second;

			// get texture variation strings (3 slots)
			std::string tex1, tex2, tex3;

			bool tex1_found = false, tex2_found = false, tex3_found = false;

			auto tvIt = row.find("TextureVariation");
			if (tvIt != row.end()) {
				auto texVec = fieldToStringVec(tvIt->second);
				if (texVec.size() > 0) { tex1 = texVec[0]; tex1_found = true; }
				if (texVec.size() > 1) { tex2 = texVec[1]; tex2_found = true; }
				if (texVec.size() > 2) { tex3 = texVec[2]; tex3_found = true; }
			}

			if (!tex1_found) {
				auto it2 = row.find("Skin1");
				if (it2 != row.end()) { tex1 = fieldToString(it2->second); tex1_found = true; }
			}
			if (!tex1_found) {
				auto it2 = row.find("field_6");
				if (it2 != row.end()) tex1 = fieldToString(it2->second);
			}
			if (!tex2_found) {
				auto it2 = row.find("Skin2");
				if (it2 != row.end()) { tex2 = fieldToString(it2->second); tex2_found = true; }
			}
			if (!tex2_found) {
				auto it2 = row.find("field_7");
				if (it2 != row.end()) tex2 = fieldToString(it2->second);
			}
			if (!tex3_found) {
				auto it2 = row.find("Skin3");
				if (it2 != row.end()) { tex3 = fieldToString(it2->second); tex3_found = true; }
			}
			if (!tex3_found) {
				auto it2 = row.find("field_8");
				if (it2 != row.end()) tex3 = fieldToString(it2->second);
			}

			// skip if no textures
			if (tex1.empty() && tex2.empty() && tex3.empty())
				continue;

			std::string model_dir = getDirectory(model_path);

			LegacyCreatureDisplay display;
			display.id = display_id;

			// build full texture paths
			if (!tex1.empty())
				display.textures.push_back(model_dir + "/" + tex1 + ".blp");
			if (!tex2.empty())
				display.textures.push_back(model_dir + "/" + tex2 + ".blp");
			if (!tex3.empty())
				display.textures.push_back(model_dir + "/" + tex3 + ".blp");

			// add to map by model path
			creatureDisplays[model_path].push_back(std::move(display));
		}

		logging::write(std::format("Loaded skin variations for {} creature models", creatureDisplays.size()));
		isInitialized = true;
	} catch (const std::exception& e) {
		logging::write(std::format("Failed to load legacy creature data: {}", e.what()));
	}
}

const std::vector<LegacyCreatureDisplay>* getCreatureDisplaysByPath(const std::string& model_path) {
	std::string normalized = model_path;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		[](unsigned char c) { return std::tolower(c); });
	std::replace(normalized.begin(), normalized.end(), '\\', '/');

	// strip MPQ archive prefix if present (e.g., "data/model.mpq/creature/...")
	static const std::regex mpq_regex(R"(\.mpq[/\\](.+))", std::regex::icase);
	std::smatch match;
	if (std::regex_search(normalized, match, mpq_regex))
		normalized = match[1].str();

	if (normalized.size() >= 4 && normalized.substr(normalized.size() - 4) == ".mdx")
		normalized = normalized.substr(0, normalized.size() - 4) + ".m2";

	auto it = creatureDisplays.find(normalized);
	if (it != creatureDisplays.end())
		return &it->second;
	return nullptr;
}

/**
 * Reset the cache (for when switching MPQ installs).
 */
void reset() {
	creatureDisplays.clear();
	isInitialized = false;
}

} // namespace db::caches::DBCreaturesLegacy
