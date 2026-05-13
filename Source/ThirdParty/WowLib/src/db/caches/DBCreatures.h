/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <functional>
#include <future>

namespace db::caches::DBCreatures {

struct CreatureDisplayInfo {
	uint32_t ID = 0;
	uint32_t modelID = 0;
	uint32_t extendedDisplayInfoID = 0;
	std::vector<uint32_t> textures;
	std::optional<std::vector<uint32_t>> extraGeosets;
};

/**
 * Initialize creature data.
 */
void initializeCreatureData();
std::future<void> initializeCreatureDataAsync();

/**
 * Gets creature skins from a given file data ID.
 * @param fileDataID File data ID of the model.
 * @returns Pointer to vector of displays, or nullptr if not found.
 */
const std::vector<std::reference_wrapper<const CreatureDisplayInfo>>* getCreatureDisplaysByFileDataID(uint32_t fileDataID);

/**
 * Gets the file data ID for a given display ID.
 * @param displayID Creature display ID.
 * @returns File data ID, or std::nullopt if not found.
 */
std::optional<uint32_t> getFileDataIDByDisplayID(uint32_t displayID);

/**
 * Gets the display info for a given display ID.
 * @param displayID Creature display ID.
 * @returns Pointer to display info, or nullptr if not found.
 */
const CreatureDisplayInfo* getDisplayInfo(uint32_t displayID);

/**
 * Gets the file data ID for a given CreatureModelData ID.
 * @param modelDataID CreatureModelData ID.
 * @returns File data ID, or std::nullopt if not found.
 */
std::optional<uint32_t> getFileDataIDByModelDataID(uint32_t modelDataID);

} // namespace db::caches::DBCreatures
