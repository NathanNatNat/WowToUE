/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
*/
#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <future>

namespace db::caches::DBComponentTextureFileData {

struct ComponentTextureInfo {
	uint32_t raceID = 0;
	uint32_t genderIndex = 0;
	uint32_t classID = 0;
};

/**
 * Initialize ComponentTextureFileData from DB2.
 */
void initialize();
std::future<void> initializeAsync();

/**
 * Filter a list of FileDataIDs to find the best texture match for race/gender.
 * @param file_data_ids List of candidate FileDataIDs.
 * @param race_id Character race ID.
 * @param gender_index 0=male, 1=female.
 * @param fallback_race_id Optional fallback race.
 * @returns Best matching FileDataID or std::nullopt.
 */
std::optional<uint32_t> getTextureForRaceGender(const std::vector<uint32_t>& file_data_ids, std::optional<uint32_t> race_id, std::optional<uint32_t> gender_index, uint32_t fallback_race_id = 0);

/**
 * Check if a FileDataID has ComponentTextureFileData entry.
 */
bool hasEntry(uint32_t file_data_id);

/**
 * Get info for a FileDataID.
 * @returns Pointer to ComponentTextureInfo, or nullptr if not found.
 */
const ComponentTextureInfo* getInfo(uint32_t file_data_id);

} // namespace db::caches::DBComponentTextureFileData
