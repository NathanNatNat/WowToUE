#include "equip-item.h"
#include "EquipmentSlots.h"
#include "../core.h"
#include "../db/caches/DBItems.h"

#include <format>
#include <vector>

namespace wow {

bool equip_item(uint32_t item_id, const std::string& item_name, int pending_slot) {
	auto slot_id_opt = db::caches::DBItems::getItemSlotId(item_id);
	if (!slot_id_opt.has_value())
		return false;

	int slot_id = slot_id_opt.value();
	if (slot_id == 0)
		return false;
	auto& view = *core::view;

	std::vector<int> slot_ids;
	if (slot_id == SHOULDER_SLOT_L) {
		int target = (pending_slot == SHOULDER_SLOT_L || pending_slot == SHOULDER_SLOT_R)
		             ? pending_slot
		             : SHOULDER_SLOT_L;
		int other = (target == SHOULDER_SLOT_L) ? SHOULDER_SLOT_R : SHOULDER_SLOT_L;

		slot_ids.push_back(target);

		std::string other_str = std::to_string(other);
		bool other_empty = !view.chrEquippedItems.is_object()
		                   || !view.chrEquippedItems.contains(other_str)
		                   || view.chrEquippedItems[other_str].is_null();
		if (other_empty)
			slot_ids.push_back(other);
	} else {
		slot_ids.push_back(slot_id);
	}

	for (int sid : slot_ids) {
		std::string sid_str = std::to_string(sid);
		view.chrEquippedItems[sid_str] = item_id;
		if (view.chrEquippedItemSkins.is_object())
			view.chrEquippedItemSkins.erase(sid_str);
	}

	int name_slot = (slot_ids.size() == 1) ? slot_ids[0] : slot_id;
	auto slot_name_opt = get_slot_name(name_slot);
	std::string slot_name = slot_name_opt.has_value() ? std::string(slot_name_opt.value()) : "Unknown";

	core::setToast("success",
	               std::format("Equipped {} to {} slot.", item_name, slot_name),
	               {}, 2000);
	return true;
}

}
