/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBCreatureDisplayExtra.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

namespace db::caches::DBCreatureDisplayExtra {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::unordered_map<uint32_t, ExtraInfo> extra_map;
static std::unordered_map<uint32_t, std::vector<CustomizationOption>> option_map;

static bool is_initialized = false;
static bool is_initializing = false;
static std::mutex init_mutex;
static std::condition_variable init_cv;

static const std::vector<CustomizationOption> empty_options;

static void _initialize() {
	logging::write("Loading creature display extra data...");

	auto extraRows = casc::db2::preloadTable("CreatureDisplayInfoExtra").getAllRows();
	for (const auto& [id, row] : extraRows) {
		ExtraInfo info;
		info.DisplayRaceID = fieldToUint32(row.at("DisplayRaceID"));
		info.DisplaySexID = fieldToUint32(row.at("DisplaySexID"));
		info.DisplayClassID = fieldToUint32(row.at("DisplayClassID"));
		info.BakeMaterialResourcesID = fieldToUint32(row.at("BakeMaterialResourcesID"));
		info.HDBakeMaterialResourcesID = fieldToUint32(row.at("HDBakeMaterialResourcesID"));
		extra_map.emplace(id, info);
	}

	auto optionRows = casc::db2::preloadTable("CreatureDisplayInfoOption").getAllRows();
	for (const auto& [_optId, row] : optionRows) {
		(void)_optId;
		uint32_t extra_id = fieldToUint32(row.at("CreatureDisplayInfoExtraID"));

		CustomizationOption opt;
		opt.optionID = fieldToUint32(row.at("ChrCustomizationOptionID"));
		opt.choiceID = fieldToUint32(row.at("ChrCustomizationChoiceID"));

		option_map[extra_id].push_back(opt);
	}

	logging::write(std::format("Loaded {} creature display extras, {} with customization options", extra_map.size(), option_map.size()));
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

std::future<void> ensureInitializedAsync() {
	return std::async(std::launch::async, [] { ensureInitialized(); });
}

const ExtraInfo* get_extra(uint32_t id) {
	auto it = extra_map.find(id);
	if (it != extra_map.end())
		return &it->second;
	return nullptr;
}

const std::vector<CustomizationOption>& get_customization_choices(uint32_t extra_id) {
	auto it = option_map.find(extra_id);
	if (it != option_map.end())
		return it->second;
	return empty_options;
}

} // namespace db::caches::DBCreatureDisplayExtra
