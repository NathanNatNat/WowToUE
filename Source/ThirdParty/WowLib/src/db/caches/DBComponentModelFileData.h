/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
*/
#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace db::caches::DBComponentModelFileData {

struct ComponentModelInfo {
	uint32_t raceID = 0;
	uint32_t genderIndex = 0;
	uint32_t classID = 0;
	uint32_t positionIndex = 0;
};

struct PositionResult {
	std::optional<uint32_t> left;
	std::optional<uint32_t> right;
};

/**
 * Initialize ComponentModelFileData from DB2.
 */
void initialize();

/**
 * Filter a list of FileDataIDs to find the best match for race/gender.
 * @param file_data_ids List of candidate FileDataIDs.
 * @param race_id Character race ID.
 * @param gender_index 0=male, 1=female.
 * @param fallback_race_id Optional fallback race.
 * @returns Best matching FileDataID or std::nullopt.
 */
std::optional<uint32_t> getModelForRaceGender(const std::vector<uint32_t>& file_data_ids, uint32_t race_id, uint32_t gender_index, uint32_t fallback_race_id = 0);

/**
 * Get two models for left/right shoulders based on PositionIndex.
 * @param file_data_ids List of candidate FileDataIDs.
 * @param race_id Character race ID.
 * @param gender_index 0=male, 1=female.
 * @returns PositionResult with left and right FileDataIDs.
 */
PositionResult getModelsForRaceGenderByPosition(const std::vector<uint32_t>& file_data_ids, uint32_t race_id, uint32_t gender_index);

/**
 * Check if a FileDataID has ComponentModelFileData entry.
 * @param file_data_id FileDataID to check.
 * @returns true if entry exists.
 */
bool hasEntry(uint32_t file_data_id);

/**
 * Get info for a FileDataID.
 * @param file_data_id FileDataID to look up.
 * @returns Pointer to ComponentModelInfo, or nullptr if not found.
 */
const ComponentModelInfo* getInfo(uint32_t file_data_id);

} // namespace db::caches::DBComponentModelFileData
