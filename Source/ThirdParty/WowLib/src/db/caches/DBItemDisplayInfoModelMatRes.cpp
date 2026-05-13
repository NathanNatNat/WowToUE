/*!
	wow.export (https://github.com/Kruithne/wow.export)
	License: MIT
 */
#include "DBItemDisplayInfoModelMatRes.h"

#include "DBTextureFileData.h"
#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_map>

namespace db::caches::DBItemDisplayInfoModelMatRes {

static std::unordered_map<uint32_t, std::vector<uint32_t>> itemDisplays;
static bool is_initialized = false;

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

/**
 * Initialize Item Display Info Model Mat Res DB2.
 */
void initialize() {
	if (is_initialized)
		return;

	DBTextureFileData::ensureInitialized();

	logging::write("Loading item display info model mat res...");

	for (const auto& [id, row] : casc::db2::preloadTable("ItemDisplayInfoModelMatRes").getAllRows()) {
		if (id == 0)
			continue;

		auto itemDisplayInfoIDIt = row.find("ItemDisplayInfoID");
		auto matresIDIt = row.find("MaterialResourcesID");
		if (itemDisplayInfoIDIt == row.end() || matresIDIt == row.end())
			continue;

		const uint32_t itemdisplayinfoid = fieldToUint32(itemDisplayInfoIDIt->second);
		const uint32_t matresid = fieldToUint32(matresIDIt->second);

		const auto* textureFileDataIDs = DBTextureFileData::getTextureFDIDsByMatID(matresid);
		if (textureFileDataIDs != nullptr) {
			auto& entry = itemDisplays[itemdisplayinfoid];
			entry.insert(entry.end(), textureFileDataIDs->begin(), textureFileDataIDs->end());
		}
	}

	logging::write(std::format("Loaded {} item display info model mat res items", itemDisplays.size()));
	is_initialized = true;
}

void ensureInitialized() {
	if (!is_initialized)
		initialize();
}

/**
 * Get texture file data IDs by ItemDisplayInfoID.
 * @param ItemDisplayInfoId ItemDisplayInfoID.
 * @returns Pointer to vector of texture file data IDs, or nullptr if not found.
 */
const std::vector<uint32_t>* getItemDisplayIdTextureFileIds(uint32_t ItemDisplayInfoId) {
	auto it = itemDisplays.find(ItemDisplayInfoId);
	if (it != itemDisplays.end())
		return &it->second;
	return nullptr;
}

} // namespace db::caches::DBItemDisplayInfoModelMatRes
