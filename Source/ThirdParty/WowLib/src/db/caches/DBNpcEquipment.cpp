/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBNpcEquipment.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>

namespace db::caches::DBNpcEquipment {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static int fieldToInt(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<int>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<int>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<int>(*p);
	return 0;
}

// NPC slot index -> our equipment slot ID
const std::unordered_map<int, int> NPC_SLOT_MAP = {
	{0, 1},   // head
	{1, 3},   // shoulder
	{2, 4},   // shirt
	{3, 5},   // chest
	{4, 6},   // waist
	{5, 7},   // legs
	{6, 8},   // feet
	{7, 9},   // wrist
	{8, 10},  // hands
	{9, 19},  // tabard
	{10, 15}  // back
};

// maps CreatureDisplayInfoExtraID -> Map<slot_id, item_display_info_id>
static std::unordered_map<uint32_t, std::unordered_map<int, uint32_t>> equipment_map;

static bool is_initialized = false;

void initialize() {
	if (is_initialized)
		return;

	logging::write("Loading NPC equipment data...");

	for (const auto& [_id, row] : casc::db2::preloadTable("NPCModelItemSlotDisplayInfo").getAllRows()) {
		(void)_id;
		uint32_t extra_id = fieldToUint32(row.at("NpcModelID"));
		int npc_slot = fieldToInt(row.at("ItemSlot"));
		uint32_t display_id = fieldToUint32(row.at("ItemDisplayInfoID"));

		auto slot_it = NPC_SLOT_MAP.find(npc_slot);
		if (slot_it == NPC_SLOT_MAP.end() || display_id == 0)
			continue;

		equipment_map[extra_id][slot_it->second] = display_id;
	}

	logging::write(std::format("Loaded NPC equipment for {} creature display extras", equipment_map.size()));
	is_initialized = true;
}

void ensureInitialized() {
	if (!is_initialized)
		initialize();
}

const std::unordered_map<int, uint32_t>* get_equipment(uint32_t extra_display_id) {
	auto it = equipment_map.find(extra_display_id);
	if (it != equipment_map.end())
		return &it->second;
	return nullptr;
}

} // namespace db::caches::DBNpcEquipment
