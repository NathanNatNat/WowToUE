/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wow {

// shoulder slot IDs (JS: SHOULDER_SLOT_L = 3, SHOULDER_SLOT_R = 30)
inline constexpr int SHOULDER_SLOT_L = 3;
inline constexpr int SHOULDER_SLOT_R = 30;

// equipment slots with numeric IDs and display names
struct EquipmentSlotEntry {
	int id;
	std::string_view name;
	std::string_view filter_name; // optional alternative name for slot filter (e.g. both shoulders share "Shoulder")
};

inline constexpr std::array<EquipmentSlotEntry, 15> EQUIPMENT_SLOTS = {{
	{ 1,               "Head",        "Head" },
	{ 2,               "Neck",        "Neck" },
	{ SHOULDER_SLOT_L, "Shoulder (L)", "Shoulder" },
	{ SHOULDER_SLOT_R, "Shoulder (R)", "Shoulder" },
	{ 15,              "Back",        "Back" },
	{ 5,               "Chest",       "Chest" },
	{ 4,               "Shirt",       "Shirt" },
	{ 19,              "Tabard",      "Tabard" },
	{ 9,               "Wrist",       "Wrist" },
	{ 10,              "Hands",       "Hands" },
	{ 6,               "Waist",       "Waist" },
	{ 7,               "Legs",        "Legs" },
	{ 8,               "Feet",        "Feet" },
	{ 16,              "Main-hand",   "Main-hand" },
	{ 17,              "Off-hand",    "Off-hand" }
}};

// maps slot ID to display name
extern const std::unordered_map<int, std::string_view> SLOT_ID_TO_NAME;

// maps inventory type id to slot id
extern const std::unordered_map<int, int> INVENTORY_TYPE_TO_SLOT_ID;

// maps WoWModelViewer CharSlots to our slot IDs
extern const std::unordered_map<int, int> WMV_SLOT_TO_SLOT_ID;

// M2 attachment IDs (from wowmodelviewer POSITION_SLOTS enum)
namespace ATTACHMENT_ID {
	constexpr int SHIELD = 0;
	constexpr int HAND_RIGHT = 1;
	constexpr int HAND_LEFT = 2;
	constexpr int ELBOW_RIGHT = 3;
	constexpr int ELBOW_LEFT = 4;
	constexpr int SHOULDER_RIGHT = 5;
	constexpr int SHOULDER_LEFT = 6;
	constexpr int KNEE_RIGHT = 7;
	constexpr int KNEE_LEFT = 8;
	constexpr int HIP_RIGHT = 9;
	constexpr int HIP_LEFT = 10;
	constexpr int HELMET = 11;
	constexpr int BACK = 12;
	constexpr int SHOULDER_FLAP_RIGHT = 13;
	constexpr int SHOULDER_FLAP_LEFT = 14;
	constexpr int BUST = 15;
	constexpr int BUST2 = 16;
	constexpr int FACE = 17;
	constexpr int ABOVE_CHARACTER = 18;
	constexpr int GROUND = 19;
	constexpr int TOP_OF_HEAD = 20;
	constexpr int LEFT_PALM2 = 21;
	constexpr int RIGHT_PALM2 = 22;
	constexpr int PRE_CAST_2L = 23;
	constexpr int PRE_CAST_2R = 24;
	constexpr int PRE_CAST_3 = 25;
	constexpr int SHEATH_MAIN_HAND = 26;
	constexpr int SHEATH_OFF_HAND = 27;
	constexpr int SHEATH_SHIELD = 28;
	constexpr int BELLY = 29;
	constexpr int LEFT_BACK = 30;
	constexpr int RIGHT_BACK = 31;
	constexpr int LEFT_HIP_SHEATH = 32;
	constexpr int RIGHT_HIP_SHEATH = 33;
	constexpr int BUST3 = 34;
	constexpr int PALM3 = 35;
	constexpr int RIGHT_PALM_UNK2 = 36;
	constexpr int LEFT_FOOT = 47;
	constexpr int RIGHT_FOOT = 48;
	constexpr int SHIELD_NO_GLOVE = 49;
	constexpr int SPINE_LOW = 50;
	constexpr int ALTERED_SHOULDER_R = 51;
	constexpr int ALTERED_SHOULDER_L = 52;
	constexpr int BELT_BUCKLE = 53;
	constexpr int SHEATH_CROSSBOW = 54;
	constexpr int HEAD_TOP = 55;
} // namespace ATTACHMENT_ID

// texture layer priority per slot
// lower values render first, higher values render on top
extern const std::unordered_map<int, int> SLOT_LAYER;

// maps equipment slot ID to M2 attachment ID(s)
// some slots have multiple attachments (e.g., shoulders have left and right)
// order matches ItemDisplayInfo.ModelResourcesID order
extern const std::unordered_map<int, std::vector<int>> SLOT_TO_ATTACHMENT;

std::optional<int> get_slot_id_for_inventory_type(int inventory_type);
std::optional<int> get_slot_id_for_wmv_slot(int wmv_slot);
std::optional<std::string_view> get_slot_name(int slot_id);
std::optional<std::span<const int>> get_attachment_ids_for_slot(int slot_id);
int get_slot_layer(int slot_id);

} // namespace wow
