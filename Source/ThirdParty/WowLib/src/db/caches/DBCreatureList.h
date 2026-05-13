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
#include <future>

namespace db::caches::DBCreatureList {

struct CreatureEntry {
	uint32_t id = 0;
	std::string name;
	uint32_t displayID = 0;
	std::vector<uint32_t> always_items;
};

/**
 * Initialize creature list from Creature.db2
 */
void initialize_creature_list();
std::future<void> initialize_creature_list_async();

/**
 * Get all creatures.
 * @returns Reference to the creature map.
 */
const std::vector<CreatureEntry>& get_all_creatures();

/**
 * Get a creature by its ID.
 * @param id Creature ID.
 * @returns Pointer to creature entry, or nullptr if not found.
 */
const CreatureEntry* get_creature_by_id(uint32_t id);

} // namespace db::caches::DBCreatureList
