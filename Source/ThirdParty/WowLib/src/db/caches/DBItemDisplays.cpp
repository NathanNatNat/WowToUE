/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "DBItemDisplays.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include "DBModelFileData.h"
#include "DBItemDisplayInfoModelMatRes.h"

#include <format>
#include <unordered_map>

namespace db::caches::DBItemDisplays {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::vector<uint32_t> fieldToUint32Vec(const db::FieldValue& val) {
	std::vector<uint32_t> result;
	if (auto* p = std::get_if<std::vector<int64_t>>(&val)) {
		result.reserve(p->size());
		for (auto v : *p)
			result.push_back(static_cast<uint32_t>(v));
	} else if (auto* p = std::get_if<std::vector<uint64_t>>(&val)) {
		result.reserve(p->size());
		for (auto v : *p)
			result.push_back(static_cast<uint32_t>(v));
	}
	return result;
}

static std::unordered_map<uint32_t, std::vector<ItemDisplay>> itemDisplays;
static bool initialized = false;

/**
 * Initialize item displays from ItemDisplayInfo.db2
 */
void initializeItemDisplays() {
	if (initialized)
		return;

	DBItemDisplayInfoModelMatRes::ensureInitialized();

	logging::write("Loading item textures...");

	// Using the texture mapping, map all model fileDataIDs to used textures.
	for (const auto& [itemDisplayInfoID, itemDisplayInfoRow] : casc::db2::preloadTable("ItemDisplayInfo").getAllRows()) {
		auto allModelResIDs = fieldToUint32Vec(itemDisplayInfoRow.at("ModelResourcesID"));
		std::vector<uint32_t> modelResIDs;
		for (auto e : allModelResIDs) {
			if (e > 0)
				modelResIDs.push_back(e);
		}
		if (modelResIDs.empty())
			continue;

		auto allMatResIDs = fieldToUint32Vec(itemDisplayInfoRow.at("ModelMaterialResourcesID"));
		std::vector<uint32_t> matResIDs;
		for (auto e : allMatResIDs) {
			if (e > 0)
				matResIDs.push_back(e);
		}
		if (matResIDs.empty())
			continue;

		const auto* modelFileDataIDs = DBModelFileData::getModelFileDataID(modelResIDs[0]);
		if (modelFileDataIDs == nullptr)
			continue;

		const auto* itemDisplayTexFileDataIDs = DBItemDisplayInfoModelMatRes::getItemDisplayIdTextureFileIds(itemDisplayInfoID);
		if (itemDisplayTexFileDataIDs == nullptr)
			continue;

		for (uint32_t modelFileDataID : *modelFileDataIDs) {
			ItemDisplay display;
			display.ID = itemDisplayInfoID;
			display.textures = *itemDisplayTexFileDataIDs;

			itemDisplays[modelFileDataID].push_back(std::move(display));
		}
	}

	logging::write(std::format("Loaded textures for {} items", itemDisplays.size()));
	initialized = true;
}

/**
 * Gets item skins from a given file data ID.
 * @param fileDataID File data ID.
 * @returns Pointer to vector of displays, or nullptr if not found.
 */
const std::vector<ItemDisplay>* getItemDisplaysByFileDataID(uint32_t fileDataID) {
	auto it = itemDisplays.find(fileDataID);
	if (it != itemDisplays.end())
		return &it->second;
	return nullptr;
}

} // namespace db::caches::DBItemDisplays