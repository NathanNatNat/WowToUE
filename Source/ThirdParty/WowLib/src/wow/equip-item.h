/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>

namespace wow {

/**
 * Equip an item onto the character.
 *
 * JS equivalent: equip_item(core, item, pending_slot) in wow/equip-item.js.
 *
 * Looks up the item's slot via DBItems::getItemSlotId. For shoulder items
 * (SHOULDER_SLOT_L), uses pending_slot to choose left or right, and also
 * fills the other shoulder slot if it is empty.
 *
 * @param item_id      Item ID to equip.
 * @param item_name    Display name (for toast message).
 * @param pending_slot Preferred slot ID from character tab (0 = none).
 * @returns true if the item was equipped, false if it cannot be equipped.
 */
bool equip_item(uint32_t item_id, const std::string& item_name, int pending_slot = 0);

} // namespace wow
