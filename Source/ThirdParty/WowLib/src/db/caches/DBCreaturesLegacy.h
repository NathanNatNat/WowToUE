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
#include <functional>

namespace db::caches::DBCreaturesLegacy {

struct LegacyCreatureDisplay {
	uint32_t id = 0;
	std::vector<std::string> textures;
};

/**
 * Initialize legacy creature display data from DBC files.
 * @param getFile Function that takes a DBC path string and returns file data (empty vector if not found).
 * @param build_id Build identifier for schema lookup.
 */
void initializeCreatureData(std::function<std::vector<uint8_t>(const std::string&)> getFile, const std::string& build_id);

/**
 * Get all available creature display variations for a model path.
 * @param model_path The M2 model path (can include MPQ prefix).
 * @returns Pointer to vector of displays, or nullptr if not found.
 */
const std::vector<LegacyCreatureDisplay>* getCreatureDisplaysByPath(const std::string& model_path);

/**
 * Reset the cache (for when switching MPQ installs).
 */
void reset();

} // namespace db::caches::DBCreaturesLegacy
