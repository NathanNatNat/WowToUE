/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "DBCreatures.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

namespace db::caches::DBCreatures {

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

static std::unordered_map<uint32_t, std::vector<std::reference_wrapper<const CreatureDisplayInfo>>> creatureDisplays;
static std::unordered_map<uint32_t, CreatureDisplayInfo> creatureDisplayInfoMap;
static std::unordered_map<uint32_t, uint32_t> displayIDToFileDataID;
static std::unordered_map<uint32_t, uint32_t> modelDataIDToFileDataID;
static bool isInitialized = false;
static bool isInitializing = false;
static std::mutex init_mutex;
static std::condition_variable init_cv;

/**
 * Initialize creature data.
 */
void initializeCreatureData() {
	{
		std::unique_lock lock(init_mutex);
		if (isInitialized)
			return;
		if (isInitializing) {
			init_cv.wait(lock, [] { return isInitialized || !isInitializing; });
			return;
		}
		isInitializing = true;
	}

	try {
		logging::write("Loading creature textures...");

		std::unordered_map<uint32_t, std::vector<uint32_t>> creatureGeosetMap;

		// CreatureDisplayInfoID => Array of geosets to enable which should only be used if CreatureModelData.CreatureDisplayInfoGeosetData != 0
		auto geosetRows = casc::db2::preloadTable("CreatureDisplayInfoGeosetData").getAllRows();
		for (const auto& [_geosetId, geosetRow] : geosetRows) {
			(void)_geosetId;
			uint32_t creatureDisplayInfoID = fieldToUint32(geosetRow.at("CreatureDisplayInfoID"));
			uint32_t geosetIndex = fieldToUint32(geosetRow.at("GeosetIndex"));
			uint32_t geosetValue = fieldToUint32(geosetRow.at("GeosetValue"));

			creatureGeosetMap[creatureDisplayInfoID].push_back((geosetIndex + 1) * 100 + geosetValue);
		}

		std::unordered_map<uint32_t, std::vector<uint32_t>> modelIDToDisplayInfoMap;

		// Map all available texture fileDataIDs to model IDs.
		auto displayRows = casc::db2::preloadTable("CreatureDisplayInfo").getAllRows();
		for (const auto& [displayID, displayRow] : displayRows) {
			uint32_t modelID = fieldToUint32(displayRow.at("ModelID"));
			uint32_t extendedDisplayInfoID = fieldToUint32(displayRow.at("ExtendedDisplayInfoID"));

			auto allTextures = fieldToUint32Vec(displayRow.at("TextureVariationFileDataID"));
			std::vector<uint32_t> textures;
			for (auto t : allTextures) {
				if (t > 0)
					textures.push_back(t);
			}

			CreatureDisplayInfo info;
			info.ID = displayID;
			info.modelID = modelID;
			info.extendedDisplayInfoID = extendedDisplayInfoID;
			info.textures = std::move(textures);
			creatureDisplayInfoMap.emplace(displayID, std::move(info));

			modelIDToDisplayInfoMap[modelID].push_back(displayID);
		}

		auto modelRows = casc::db2::preloadTable("CreatureModelData").getAllRows();
		for (const auto& [modelID, modelRow] : modelRows) {
			uint32_t fileDataID = fieldToUint32(modelRow.at("FileDataID"));
			modelDataIDToFileDataID.emplace(modelID, fileDataID);

			auto it = modelIDToDisplayInfoMap.find(modelID);
			if (it != modelIDToDisplayInfoMap.end()) {
				uint32_t creatureGeosetDataID = fieldToUint32(modelRow.at("CreatureGeosetDataID"));
				bool modelIDHasExtraGeosets = creatureGeosetDataID > 0;
				const auto& displayIDs = it->second;

				for (const auto displayID : displayIDs) {
					displayIDToFileDataID.emplace(displayID, fileDataID);

					auto& display = creatureDisplayInfoMap.at(displayID);

					if (modelIDHasExtraGeosets) {
						display.extraGeosets.emplace();
						auto geoIt = creatureGeosetMap.find(displayID);
						if (geoIt != creatureGeosetMap.end())
							display.extraGeosets = geoIt->second;
					}

					creatureDisplays[fileDataID].push_back(std::cref(display));
				}
			}
		}

		logging::write(std::format("Loaded textures for {} creatures", creatureDisplays.size()));
		{
			std::scoped_lock lock(init_mutex);
			isInitialized = true;
			isInitializing = false;
		}
		init_cv.notify_all();
	} catch (...) {
		{
			std::scoped_lock lock(init_mutex);
			isInitializing = false;
		}
		init_cv.notify_all();
		throw;
	}
}

std::future<void> initializeCreatureDataAsync() {
	return std::async(std::launch::async, [] { initializeCreatureData(); });
}

const std::vector<std::reference_wrapper<const CreatureDisplayInfo>>* getCreatureDisplaysByFileDataID(uint32_t fileDataID) {
	auto it = creatureDisplays.find(fileDataID);
	if (it != creatureDisplays.end())
		return &it->second;
	return nullptr;
}

/**
 * Gets the file data ID for a given display ID.
 */
std::optional<uint32_t> getFileDataIDByDisplayID(uint32_t displayID) {
	auto it = displayIDToFileDataID.find(displayID);
	if (it != displayIDToFileDataID.end())
		return it->second;
	return std::nullopt;
}

const CreatureDisplayInfo* getDisplayInfo(uint32_t displayID) {
	auto it = creatureDisplayInfoMap.find(displayID);
	if (it != creatureDisplayInfoMap.end())
		return &it->second;
	return nullptr;
}

std::optional<uint32_t> getFileDataIDByModelDataID(uint32_t modelDataID) {
	auto it = modelDataIDToFileDataID.find(modelDataID);
	if (it != modelDataIDToFileDataID.end())
		return it->second;
	return std::nullopt;
}

} // namespace db::caches::DBCreatures
