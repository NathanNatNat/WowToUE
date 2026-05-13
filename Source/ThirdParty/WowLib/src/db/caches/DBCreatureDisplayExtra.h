/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>
#include <future>

namespace db::caches::DBCreatureDisplayExtra {

struct ExtraInfo {
	uint32_t DisplayRaceID = 0;
	uint32_t DisplaySexID = 0;
	uint32_t DisplayClassID = 0;
	uint32_t BakeMaterialResourcesID = 0;
	uint32_t HDBakeMaterialResourcesID = 0;
};

struct CustomizationOption {
	uint32_t optionID = 0;
	uint32_t choiceID = 0;
};

/**
 * Ensure creature display extra data is initialized.
 */
void ensureInitialized();
std::future<void> ensureInitializedAsync();

/**
 * Get extra display info by ID.
 * @param id Display info extra ID.
 * @returns Pointer to ExtraInfo, or nullptr if not found.
 */
const ExtraInfo* get_extra(uint32_t id);

/**
 * Get customization choices for a creature display extra.
 * @param extra_id Display info extra ID.
 * @returns Vector of customization options (empty if none).
 */
const std::vector<CustomizationOption>& get_customization_choices(uint32_t extra_id);

} // namespace db::caches::DBCreatureDisplayExtra
