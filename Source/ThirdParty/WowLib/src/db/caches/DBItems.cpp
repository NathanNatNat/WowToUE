/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBItems.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../../wow/EquipmentSlots.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_map>

namespace db::caches::DBItems {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::string fieldToString(const db::FieldValue& val) {
	if (auto* p = std::get_if<std::string>(&val))
		return *p;
	return "";
}

static std::unordered_map<uint32_t, ItemInfo> items_by_id;
static bool is_initialized_flag = false;

// item class 2 = Weapon, subclass 2 = Bow
static constexpr uint32_t ITEM_CLASS_WEAPON = 2;
static constexpr uint32_t ITEM_SUBCLASS_BOW = 2;

void initialize() {
	if (is_initialized_flag)
		return;

	logging::write("Loading item cache...");

	// load item class/subclass from Item table
	std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> item_class_map;
	auto item_rows = casc::db2::preloadTable("Item").getAllRows();
	for (const auto& [item_id, item_row] : item_rows) {
		uint32_t classID = fieldToUint32(item_row.at("ClassID"));
		uint32_t subclassID = fieldToUint32(item_row.at("SubclassID"));
		item_class_map[item_id] = {classID, subclassID};
	}

	auto item_sparse_rows = casc::db2::preloadTable("ItemSparse").getAllRows();

	for (const auto& [item_id, item_row] : item_sparse_rows) {
		ItemInfo info;
		info.id = item_id;

		auto nameIt = item_row.find("Display_lang");
		if (nameIt != item_row.end())
			info.name = fieldToString(nameIt->second);
		else
			info.name = std::format("Unknown item #{}", item_id);

		info.inventoryType = fieldToUint32(item_row.at("InventoryType"));

		auto qualIt = item_row.find("OverallQualityID");
		info.quality = (qualIt != item_row.end()) ? fieldToUint32(qualIt->second) : 0;

		auto classIt = item_class_map.find(item_id);
		if (classIt != item_class_map.end()) {
			info.classID = classIt->second.first;
			info.subclassID = classIt->second.second;
		}

		items_by_id.emplace(item_id, std::move(info));
	}

	logging::write(std::format("Loaded {} items into cache", items_by_id.size()));
	is_initialized_flag = true;
}

void ensureInitialized() {
	if (!is_initialized_flag)
		initialize();
}

const ItemInfo* getItemById(uint32_t item_id) {
	auto it = items_by_id.find(item_id);
	if (it != items_by_id.end())
		return &it->second;
	return nullptr;
}

std::optional<int> getItemSlotId(uint32_t item_id) {
	auto it = items_by_id.find(item_id);
	if (it == items_by_id.end())
		return std::nullopt;

	return wow::get_slot_id_for_inventory_type(static_cast<int>(it->second.inventoryType));
}

bool isInitialized() {
	return is_initialized_flag;
}

bool isItemBow(uint32_t item_id) {
	auto it = items_by_id.find(item_id);
	if (it == items_by_id.end())
		return false;

	return it->second.classID == ITEM_CLASS_WEAPON && it->second.subclassID == ITEM_SUBCLASS_BOW;
}

} // namespace db::caches::DBItems
