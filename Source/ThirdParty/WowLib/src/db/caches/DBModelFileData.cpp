/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "DBModelFileData.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_map>

namespace db::caches::DBModelFileData {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::unordered_map<uint32_t, std::vector<uint32_t>> modelResIDToFileDataID;
static std::unordered_set<uint32_t> fileDataIDs;
static bool initialized = false;

/**
 * Initialize model file data from ModelFileData.db2
 */
void initializeModelFileData() {
	if (initialized)
		return;

	logging::write("Loading model mapping...");

	auto allRows = casc::db2::preloadTable("ModelFileData").getAllRows();
	for (const auto& [modelFileDataID, modelFileDataRow] : allRows) {
		// Keep a list of all FIDs for listfile unknowns.
		fileDataIDs.insert(modelFileDataID);

		uint32_t modelResourcesID = fieldToUint32(modelFileDataRow.at("ModelResourcesID"));

		auto it = modelResIDToFileDataID.find(modelResourcesID);
		if (it != modelResIDToFileDataID.end())
			it->second.push_back(modelFileDataID);
		else
			modelResIDToFileDataID.emplace(modelResourcesID, std::vector<uint32_t>{modelFileDataID});
	}

	logging::write(std::format("Loaded model mapping for {} models", modelResIDToFileDataID.size()));
	initialized = true;
}

/**
 * Retrieve a model file data ID.
 * @param modelResID Model resource ID to look up.
 * @returns Pointer to vector of file data IDs, or nullptr if not found.
 */
const std::vector<uint32_t>* getModelFileDataID(uint32_t modelResID) {
	auto it = modelResIDToFileDataID.find(modelResID);
	if (it != modelResIDToFileDataID.end())
		return &it->second;
	return nullptr;
}

/**
 * Retrieve a list of all file data IDs cached from ModelFileData.db2
 * NOTE: This is reset once called by the listfile module; adjust if needed elsewhere.
 * @returns Reference to the set of all model file data IDs.
 */
const std::unordered_set<uint32_t>& getFileDataIDs() {
	return fileDataIDs;
}

} // namespace db::caches::DBModelFileData