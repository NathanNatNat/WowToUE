/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "ItemSlot.h"

#include <array>
#include <string_view>

namespace wow {

namespace {

constexpr std::array<std::string_view, 29> ITEM_SLOTS = {
	"Non-equippable",
	"Head",
	"Neck",
	"Shoulder",
	"Shirt",
	"Chest",
	"Waist",
	"Legs",
	"Feet",
	"Wrist",
	"Hands",
	"Finger",
	"Trinket",
	"One-hand",
	"Off-hand",
	"Ranged",
	"Back",
	"Two-hand",
	"Bag",
	"Tabard",
	"Chest",
	"Main-hand",
	"Off-hand",
	"Off-hand",
	"Ammo",
	"Thrown",
	"Ranged",
	"Quiver",
	"Relic"
};

}

/**
 * Get the label for an item slot based on the ID.
 * @param id
 */
std::string_view getSlotName(int id) {
	if (id >= 0 && id < static_cast<int>(ITEM_SLOTS.size()))
		return ITEM_SLOTS[id];
	return "Unknown";
}

}
