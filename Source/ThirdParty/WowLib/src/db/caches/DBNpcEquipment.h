/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <unordered_map>
#include <optional>

namespace db::caches::DBNpcEquipment {

// NPC slot index -> our equipment slot ID
extern const std::unordered_map<int, int> NPC_SLOT_MAP;

/**
 * Initialize NPC equipment data from DB2.
 */
void initialize();

/**
 * Ensure NPC equipment data is initialized.
 */
void ensureInitialized();

/**
 * Get equipment for a CreatureDisplayInfoExtraID.
 * @param extra_display_id CreatureDisplayInfoExtraID.
 * @returns Pointer to map of slot_id -> item_display_info_id, or nullptr.
 */
const std::unordered_map<int, uint32_t>* get_equipment(uint32_t extra_display_id);

} // namespace db::caches::DBNpcEquipment
