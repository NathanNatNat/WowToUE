/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "DBTextureFileData.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_map>

namespace db::caches::DBTextureFileData {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::unordered_map<uint32_t, std::vector<uint32_t>> matResIDToFileDataID;
static std::unordered_set<uint32_t> fileDataIDs;

/**
 * Initialize texture file data ID from TextureFileData.db2
 */
void initializeTextureFileData() {
	logging::write("Loading texture mapping...");

	auto allRows = casc::db2::preloadTable("TextureFileData").getAllRows();
	for (const auto& [textureFileDataID, textureFileDataRow] : allRows) {
		// Keep a list of all FIDs for listfile unknowns.
		fileDataIDs.insert(textureFileDataID);

		// TODO: Need to remap this to support other UsageTypes
		uint32_t usageType = fieldToUint32(textureFileDataRow.at("UsageType"));
		if (usageType != 0)
			continue;

		uint32_t materialResourcesID = fieldToUint32(textureFileDataRow.at("MaterialResourcesID"));

		auto it = matResIDToFileDataID.find(materialResourcesID);
		if (it != matResIDToFileDataID.end())
			it->second.push_back(textureFileDataID);
		else
			matResIDToFileDataID.emplace(materialResourcesID, std::vector<uint32_t>{textureFileDataID});
	}

	logging::write(std::format("Loaded texture mapping for {} materials", matResIDToFileDataID.size()));
}

/**
 * Retrieves texture file data IDs by a material resource ID.
 * @param matResID Material resource ID.
 * @returns Pointer to vector of file data IDs, or nullptr if not found.
 */
const std::vector<uint32_t>* getTextureFDIDsByMatID(uint32_t matResID) {
	auto it = matResIDToFileDataID.find(matResID);
	if (it != matResIDToFileDataID.end())
		return &it->second;
	return nullptr;
}

/**
 * Ensure texture file data is initialized. Call this before using other functions.
 */
void ensureInitialized() {
	if (matResIDToFileDataID.empty())
		initializeTextureFileData();
}

/**
 * Retrieve a list of all file data IDs cached from TextureFileData.db2
 * NOTE: This is reset once called by the listfile module; adjust if needed elsewhere.
 * @returns Reference to the set of all texture file data IDs.
 */
const std::unordered_set<uint32_t>& getFileDataIDs() {
	return fileDataIDs;
}

} // namespace db::caches::DBTextureFileData