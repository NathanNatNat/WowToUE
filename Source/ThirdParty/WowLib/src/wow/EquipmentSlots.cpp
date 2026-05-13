/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "EquipmentSlots.h"

namespace wow {

// maps slot ID to display name
const std::unordered_map<int, std::string_view> SLOT_ID_TO_NAME = [] {
	std::unordered_map<int, std::string_view> map;
	for (const auto& slot : EQUIPMENT_SLOTS)
		map[slot.id] = slot.name;
	return map;
}();

// maps inventory type id to slot id
const std::unordered_map<int, int> INVENTORY_TYPE_TO_SLOT_ID = {
	{1, 1},              // head
	{2, 2},              // neck
	{3, SHOULDER_SLOT_L}, // shoulder -> left shoulder
	{4, 4},   // shirt
	{5, 5},   // chest
	{6, 6},   // waist
	{7, 7},   // legs
	{8, 8},   // feet
	{9, 9},   // wrist
	{10, 10}, // hands
	{13, 16}, // one-hand -> main-hand
	{14, 17}, // shield -> off-hand
	{15, 16}, // ranged -> main-hand
	{16, 15}, // back
	{17, 16}, // two-hand -> main-hand
	{19, 19}, // tabard
	{20, 5},  // robe -> chest
	{21, 16}, // main-hand
	{22, 17}, // off-hand weapon
	{23, 17}, // holdable -> off-hand
	{26, 16}  // ranged right -> main-hand
};

// maps WoWModelViewer CharSlots to our slot IDs
const std::unordered_map<int, int> WMV_SLOT_TO_SLOT_ID = {
	{0, 1},             // CS_HEAD -> head
	{1, SHOULDER_SLOT_L}, // CS_SHOULDER -> left shoulder
	{2, 8},   // CS_BOOTS -> feet
	{3, 6},   // CS_BELT -> waist
	{4, 4},   // CS_SHIRT -> shirt
	{5, 7},   // CS_PANTS -> legs
	{6, 5},   // CS_CHEST -> chest
	{7, 9},   // CS_BRACERS -> wrist
	{8, 10},  // CS_GLOVES -> hands
	{9, 16},  // CS_HAND_RIGHT -> main-hand
	{10, 17}, // CS_HAND_LEFT -> off-hand
	{11, 15}, // CS_CAPE -> back
	{12, 19}  // CS_TABARD -> tabard
};

// texture layer priority per slot
// lower values render first, higher values render on top
const std::unordered_map<int, int> SLOT_LAYER = {
	{4, 10},            // shirt
	{7, 10},            // legs/pants
	{1, 11},            // head
	{8, 11},            // feet/boots
	{SHOULDER_SLOT_L, 13}, // shoulder (L)
	{SHOULDER_SLOT_R, 13}, // shoulder (R)
	{5, 13},            // chest
	{19, 17},           // tabard
	{6, 18},            // waist/belt
	{9, 19},            // wrist/bracers
	{10, 20},           // hands/gloves
	{16, 21},           // main-hand
	{17, 22},           // off-hand
	{15, 23}            // back/cape
};

// maps equipment slot ID to M2 attachment ID(s)
// some slots have multiple attachments (e.g., shoulders have left and right)
// order matches ItemDisplayInfo.ModelResourcesID order
const std::unordered_map<int, std::vector<int>> SLOT_TO_ATTACHMENT = {
	{1, {ATTACHMENT_ID::HELMET}},                              // head
	{SHOULDER_SLOT_L, {ATTACHMENT_ID::SHOULDER_LEFT}},         // shoulder (L)
	{SHOULDER_SLOT_R, {ATTACHMENT_ID::SHOULDER_RIGHT}},        // shoulder (R)
	{15, {ATTACHMENT_ID::BACK}},                               // back/cape
	{16, {ATTACHMENT_ID::HAND_RIGHT}},                         // main-hand weapon
	{17, {ATTACHMENT_ID::HAND_LEFT, ATTACHMENT_ID::SHIELD}}    // off-hand (weapon or shield)
};

std::optional<int> get_slot_id_for_inventory_type(int inventory_type) {
	auto it = INVENTORY_TYPE_TO_SLOT_ID.find(inventory_type);
	if (it != INVENTORY_TYPE_TO_SLOT_ID.end())
		return it->second;
	return std::nullopt;
}

std::optional<int> get_slot_id_for_wmv_slot(int wmv_slot) {
	auto it = WMV_SLOT_TO_SLOT_ID.find(wmv_slot);
	if (it != WMV_SLOT_TO_SLOT_ID.end())
		return it->second;
	return std::nullopt;
}

std::optional<std::string_view> get_slot_name(int slot_id) {
	auto it = SLOT_ID_TO_NAME.find(slot_id);
	if (it != SLOT_ID_TO_NAME.end())
		return it->second;
	return std::nullopt;
}

std::optional<std::span<const int>> get_attachment_ids_for_slot(int slot_id) {
	auto it = SLOT_TO_ATTACHMENT.find(slot_id);
	if (it != SLOT_TO_ATTACHMENT.end())
		return std::span<const int>(it->second);
	return std::nullopt;
}

int get_slot_layer(int slot_id) {
	auto it = SLOT_LAYER.find(slot_id);
	if (it != SLOT_LAYER.end())
		return it->second;
	return 10;
}

}
