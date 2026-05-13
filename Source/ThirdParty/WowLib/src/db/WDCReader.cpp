/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#include "WDCReader.h"
#include "DBDParser.h"
#include "FieldType.h"
#include "CompressionType.h"

#include "../log.h"
#include "../core.h"
#include "../generics.h"
#include "../constants.h"
#include "../buffer.h"
#include "../casc/export-helper.h"
#include "../casc/casc-source.h"
#include "../casc/casc-source-local.h"
#include "../casc/casc-source-remote.h"
#include "../casc/build-cache.h"

#include <cstdint>
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <memory>
#include <limits>
#include <cctype>

namespace db {

struct TableFormat {
	const char* name;
	int wdcVersion;
};

static const std::unordered_map<uint32_t, TableFormat> TABLE_FORMATS = {
	{ 0x32434457, { "WDC2", 2 } },
	{ 0x434C5331, { "CLS1", 2 } },
	{ 0x33434457, { "WDC3", 3 } },
	{ 0x34434457, { "WDC4", 4 } },
	{ 0x35434457, { "WDC5", 5 } },
};

static uint32_t getSectionRecordCount(const WDCSectionHeader& header) {
	return std::visit([](const auto& h) -> uint32_t { return h.recordCount; }, header);
}

static uint64_t getSectionTactKeyHash(const WDCSectionHeader& header) {
	return std::visit([](const auto& h) -> uint64_t { return h.tactKeyHash; }, header);
}

static uint32_t getSectionStringTableSize(const WDCSectionHeader& header) {
	return std::visit([](const auto& h) -> uint32_t { return h.stringTableSize; }, header);
}

static uint32_t getSectionIdListSize(const WDCSectionHeader& header) {
	return std::visit([](const auto& h) -> uint32_t { return h.idListSize; }, header);
}

static uint32_t getSectionRelationshipDataSize(const WDCSectionHeader& header) {
	return std::visit([](const auto& h) -> uint32_t { return h.relationshipDataSize; }, header);
}

static bool allZero(const std::vector<uint32_t>& v) {
	return std::all_of(v.begin(), v.end(), [](uint32_t id) { return id == 0; });
}

static int64_t fieldValueToInt64(const FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return *p;
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<int64_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<int64_t>(*p);
	return 0;
}

static std::optional<uint32_t> parseIntCoerceUint32(std::string_view input) {
	size_t i = 0;
	while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i])))
		i++;

	if (i >= input.size())
		return std::nullopt;

	int sign = 1;
	if (input[i] == '+' || input[i] == '-') {
		if (input[i] == '-')
			sign = -1;
		i++;
	}

	if (i >= input.size() || !std::isdigit(static_cast<unsigned char>(input[i])))
		return std::nullopt;

	uint64_t value = 0;
	while (i < input.size() && std::isdigit(static_cast<unsigned char>(input[i]))) {
		value = value * 10 + static_cast<uint64_t>(input[i] - '0');
		if (value > std::numeric_limits<uint32_t>::max())
			value = std::numeric_limits<uint32_t>::max();
		i++;
	}

	if (sign < 0)
		return static_cast<uint32_t>(-static_cast<int64_t>(value));

	return static_cast<uint32_t>(value);
}

static casc::BuildCache* getActiveCascCache() {
	if (!core::view || !core::view->casc)
		return nullptr;

	if (auto* local = dynamic_cast<casc::CASCLocal*>(core::view->casc))
		return local->cache;

	if (auto* remote = dynamic_cast<casc::CASCRemote*>(core::view->casc))
		return remote->cache;

	return nullptr;
}

/**
 * Returns the schema type for a DBD field.
 * @param entry The DBD field entry.
 * @returns Corresponding FieldType.
 */
FieldType convertDBDToSchemaType(const DBDField& entry) {
	if (!entry.isInline && entry.isRelation)
		return FieldType::Relation;

	if (!entry.isInline && entry.isID)
		return FieldType::NonInlineID;

	// TODO: Handle string separate to locstring in the event we need it.
	if (entry.type == "string" || entry.type == "locstring")
		return FieldType::String;

	if (entry.type == "float")
		return FieldType::Float;

	if (entry.type == "int") {
		switch (entry.size) {
			case 8: return entry.isSigned ? FieldType::Int8 : FieldType::UInt8;
			case 16: return entry.isSigned ? FieldType::Int16 : FieldType::UInt16;
			case 32: return entry.isSigned ? FieldType::Int32 : FieldType::UInt32;
			case 64: return entry.isSigned ? FieldType::Int64 : FieldType::UInt64;
			default: throw std::runtime_error("Unsupported DBD integer size " + std::to_string(entry.size));
		}
	}

	throw std::runtime_error("Unrecognized DBD type " + entry.type);
}

/**
 * Construct a new WDCReader instance.
 * @param fileName Name of the DB file.
 */
WDCReader::WDCReader(const std::string& fileName)
	: fileName(fileName)
{
}

/**
 * Returns the amount of rows available in the table.
 */
size_t WDCReader::size() const {
	return static_cast<size_t>(totalRecordCount) + copyTable.size();
}

/**
 * Get a row from this table.
 * Returns std::nullopt if the row does not exist.
 * @param recordID The record ID to look up.
 */
std::optional<DataRecord> WDCReader::getRow(uint32_t recordID) {
	if (!isLoaded)
		throw std::runtime_error("Attempted to read a data table row before table was loaded.");

	// check copy table first
	auto copyIt = copyTable.find(recordID);
	if (copyIt != copyTable.end()) {
		auto copy = _readRecord(copyIt->second);
		if (copy.has_value()) {
			DataRecord tempCopy = copy.value();
			tempCopy["ID"] = static_cast<int64_t>(recordID);
			return tempCopy;
		}
	}

	// read record directly
	return _readRecord(recordID);
}

std::optional<DataRecord> WDCReader::getRow(std::string_view recordID) {
	auto coerced = parseIntCoerceUint32(recordID);
	if (!coerced.has_value())
		return std::nullopt;

	return getRow(*coerced);
}

/**
 * Returns all available rows in the table.
 * If preload() was called, returns cached rows. Otherwise computes fresh.
 * Iterates sequentially through all sections for efficient paging with mmap.
 */
const std::map<uint32_t, DataRecord>& WDCReader::getAllRows() {
	if (!isLoaded)
		throw std::runtime_error("Attempted to read a data table rows before table was loaded.");

	// return preloaded cache if available
	if (rows.has_value())
		return *rows;

	transientRows.clear();
	auto& result = transientRows;

	// iterate through all sections sequentially
	for (size_t sectionIndex = 0; sectionIndex < sections.size(); sectionIndex++) {
		const auto& section = sections[sectionIndex];
		uint32_t headerRecordCount = getSectionRecordCount(section.header);

		// skip encrypted sections
		if (section.isEncrypted)
			continue;

		const bool hasIDMap = !section.idList.empty();
		const bool emptyIDMap = hasIDMap && allZero(section.idList);

		for (uint32_t i = 0; i < headerRecordCount; i++) {
			uint32_t recordID = 0;
			bool hasKnownID = false;

			if (hasIDMap && emptyIDMap) {
				recordID = i;
				hasKnownID = true;
			} else if (hasIDMap) {
				recordID = section.idList[i];
				hasKnownID = true;
			}

			// if no ID map, recordID will be determined during record parsing from inline ID field
			auto record = _readRecordFromSection(sectionIndex, i, recordID, hasKnownID);
			if (record.has_value()) {
				// use the ID from the record if we didn't have it upfront
				uint32_t finalRecordID;
				if (hasKnownID) {
					finalRecordID = recordID;
				} else {
					if (!idField.has_value())
						continue;

					auto idIt = record.value().find(*idField);
					if (idIt == record.value().end())
						continue;
					finalRecordID = static_cast<uint32_t>(fieldValueToInt64(idIt->second));
				}
				result[finalRecordID] = std::move(record.value());
			}
		}
	}

	// inflate copy table
	for (const auto& [destID, srcID] : copyTable) {
		auto srcIt = result.find(srcID);
		if (srcIt != result.end()) {
			DataRecord rowData = srcIt->second;
			rowData["ID"] = static_cast<int64_t>(destID);
			result[destID] = std::move(rowData);
		}
	}

	return result;
}

/**
 * Preload all rows into memory cache.
 * Subsequent calls to getAllRows() will return cached data.
 * Required for getRelationRows() to work properly.
 */
void WDCReader::preload() {
	if (!isLoaded)
		throw std::runtime_error("Attempted to preload table before it was loaded.");

	if (rows.has_value())
		return;

	rows = getAllRows();
}

/**
 * Get rows by foreign key value (uses relationship maps).
 * Returns empty vector if no rows found or table has no relationship data.
 * @param foreignKeyValue The FK value to search for.
 */
std::vector<DataRecord> WDCReader::getRelationRows(uint32_t foreignKeyValue) {
	if (!isLoaded)
		throw std::runtime_error("Attempted to query relationship data before table was loaded.");

	auto lookupIt = relationshipLookup.find(foreignKeyValue);
	if (lookupIt == relationshipLookup.end() || lookupIt->second.empty())
		return {};

	std::vector<DataRecord> results;
	for (uint32_t recordID : lookupIt->second) {
		auto row = _readRecord(recordID);
		if (row.has_value())
			results.push_back(std::move(row.value()));
	}

	return results;
}

std::vector<DataRecord> WDCReader::getRelationRows(std::string_view foreignKeyValue) {
	auto coerced = parseIntCoerceUint32(foreignKeyValue);
	if (!coerced.has_value())
		return {};

	return getRelationRows(*coerced);
}

/**
 * Load the schema for this table.
 * @param layoutHash The layout hash string.
 */
void WDCReader::loadSchema(const std::string& layoutHash) {
	auto* casc = core::view->casc;
	const std::string buildID = casc ? casc->getBuildName() : "";

	std::filesystem::path fileBaseName = std::filesystem::path(fileName).stem();
	const std::string tableName = casc::ExportHelper::replaceExtension(fileBaseName.string());
	const std::string dbdName = tableName + ".dbd";

	const DBDEntry* structure = nullptr;
	logging::write("Loading table definitions " + dbdName + " (" + buildID + " " + layoutHash + ")...");

	std::filesystem::path dbdCachePath = constants::CACHE::DIR_DBD() / dbdName;
	std::unique_ptr<DBDParser> dbdParser;
	casc::BuildCache* cache = getActiveCascCache();

	if (cache != nullptr) {
		auto cachedRaw = cache->getFile(dbdName, constants::CACHE::DIR_DBD().string());
		if (cachedRaw.has_value()) {
			dbdParser = std::make_unique<DBDParser>(cachedRaw.value());
			structure = dbdParser->getStructure(buildID, layoutHash);
		}
	} else if (std::filesystem::exists(dbdCachePath)) {
		BufferWrapper rawDbd = BufferWrapper::readFile(dbdCachePath);
		dbdParser = std::make_unique<DBDParser>(rawDbd);
		structure = dbdParser->getStructure(buildID, layoutHash);
	}

	// download if not cached
	if (structure == nullptr) {
		const std::string configDbdURL = core::view->config.value("dbdURL", "");
		const std::string configDbdFallbackURL = core::view->config.value("dbdFallbackURL", "");

		auto formatURL = [](const std::string& tmpl, const std::string& name) -> std::string {
			std::string result = tmpl;
			auto pos = result.find("%s");
			if (pos != std::string::npos)
				result.replace(pos, 2, name);
			return result;
		};

		const std::string dbd_url = formatURL(configDbdURL, tableName);
		const std::string dbd_url_fallback = formatURL(configDbdFallbackURL, tableName);

		try {
			logging::write("No cached DBD, downloading new from " + dbd_url);
			BufferWrapper rawDbd = generics::downloadFile({ dbd_url, dbd_url_fallback });

			if (cache != nullptr) {
				cache->storeFile(dbdName, rawDbd, constants::CACHE::DIR_DBD().string());
			} else {
				std::filesystem::create_directories(constants::CACHE::DIR_DBD());
				rawDbd.writeToFile(dbdCachePath);
			}

			dbdParser = std::make_unique<DBDParser>(rawDbd);
			structure = dbdParser->getStructure(buildID, layoutHash);
		} catch (const std::exception& e) {
			logging::write(e.what());
			throw std::runtime_error("Unable to download DBD for " + tableName);
		}
	}

	if (structure == nullptr)
		throw std::runtime_error("No table definition available for " + tableName);

	buildSchemaFromDBDStructure(*structure);
}

/**
 * Builds a schema for this data table using the provided DBD structure.
 * @param structure The DBD entry structure.
 */
void WDCReader::buildSchemaFromDBDStructure(const DBDEntry& structure) {
	for (const auto& field : structure.fields) {
		FieldType fieldType = convertDBDToSchemaType(field);
		if (field.arrayLength > -1) {
			schema[field.name] = std::make_pair(fieldType, field.arrayLength);
		} else {
			schema[field.name] = fieldType;
		}
		schemaOrder.push_back(field.name);
	}
}

/**
 * Gets index of ID field.
 */
uint16_t WDCReader::getIDIndex() const {
	if (!isLoaded)
		throw std::runtime_error("Attempted to get ID index before table was loaded.");

	return idFieldIndex;
}

/**
 * Parse the DB file from CASC.
 */
void WDCReader::parse() {
	logging::write("Loading DB file " + fileName + " from CASC");

	dataOwner = std::make_unique<BufferWrapper>(core::view->casc->getVirtualFileByName(fileName, false));
	data = dataOwner.get();
	auto& dataRef = *data;

	// store reference for lazy-loading
	castBuffer = std::make_unique<BufferWrapper>(BufferWrapper::alloc(8, true));

	// wdc_magic
	const uint32_t magic = dataRef.readUInt32LE();
	auto formatIt = TABLE_FORMATS.find(magic);

	if (formatIt == TABLE_FORMATS.end())
		throw std::runtime_error("Unsupported DB2 type: " + std::to_string(magic));

	const auto& format = formatIt->second;
	const int wdcVer = format.wdcVersion;
	logging::write("Processing DB file " + fileName + " as " + format.name);

	// Skip over WDC5 specific information for now
	if (wdcVer == 5) {
		dataRef.readUInt32LE(); // Schema version?
		dataRef.readUInt8(128); // Schema build string
	}

	// wdc_db2_header
	recordCount = dataRef.readUInt32LE();
	dataRef.move(4); // fieldCount
	recordSize = dataRef.readUInt32LE();
	dataRef.move(4); // stringTableSize
	dataRef.move(4); // tableHash

	auto layoutHashBytes = dataRef.readUInt8(4);
	std::reverse(layoutHashBytes.begin(), layoutHashBytes.end());
	std::ostringstream layoutHashStream;
	for (uint8_t b : layoutHashBytes)
		layoutHashStream << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
	const std::string layoutHash = layoutHashStream.str();

	minID = dataRef.readUInt32LE();
	maxID = dataRef.readUInt32LE();
	dataRef.move(4); // locale
	flags = dataRef.readUInt16LE();
	const uint16_t idIndex = dataRef.readUInt16LE();
	idFieldIndex = idIndex;
	const uint32_t totalFieldCount = dataRef.readUInt32LE();
	dataRef.move(4); // bitpackedDataOffset
	dataRef.move(4); // lookupColumnCount
	const uint32_t fieldStorageInfoSize = dataRef.readUInt32LE();
	const uint32_t commonDataSize = dataRef.readUInt32LE();
	const uint32_t palletDataSize = dataRef.readUInt32LE();
	const uint32_t sectionCount = dataRef.readUInt32LE();

	wdcVersion = wdcVer;

	// Load the DBD and parse a schema from it.
	loadSchema(layoutHash);

	// wdc_section_header section_headers[section_count]
	std::vector<WDCSectionHeader> sectionHeaders(sectionCount);
	for (uint32_t i = 0; i < sectionCount; i++) {
		if (wdcVer == 2) {
			WDCSectionHeaderV2 h;
			h.tactKeyHash = dataRef.readUInt64LE();
			h.fileOffset = dataRef.readUInt32LE();
			h.recordCount = dataRef.readUInt32LE();
			h.stringTableSize = dataRef.readUInt32LE();
			h.copyTableSize = dataRef.readUInt32LE();
			h.offsetMapOffset = dataRef.readUInt32LE();
			h.idListSize = dataRef.readUInt32LE();
			h.relationshipDataSize = dataRef.readUInt32LE();
			sectionHeaders[i] = h;
		} else {
			WDCSectionHeaderV3 h;
			h.tactKeyHash = dataRef.readUInt64LE();
			h.fileOffset = dataRef.readUInt32LE();
			h.recordCount = dataRef.readUInt32LE();
			h.stringTableSize = dataRef.readUInt32LE();
			h.offsetRecordsEnd = dataRef.readUInt32LE();
			h.idListSize = dataRef.readUInt32LE();
			h.relationshipDataSize = dataRef.readUInt32LE();
			h.offsetMapIDCount = dataRef.readUInt32LE();
			h.copyTableCount = dataRef.readUInt32LE();
			sectionHeaders[i] = h;
		}
	}

	// fields[header.total_field_count]
	for (uint32_t i = 0; i < totalFieldCount; i++) {
		dataRef.readInt16LE();  // size
		dataRef.readUInt16LE(); // position
	}

	// field_info[header.field_storage_info_size / sizeof(field_storage_info)]
	const uint32_t fieldInfoCount = fieldStorageInfoSize / (4 * 6);
	fieldInfo.resize(fieldInfoCount);
	for (uint32_t i = 0; i < fieldInfoCount; i++) {
		fieldInfo[i].fieldOffsetBits = dataRef.readUInt16LE();
		fieldInfo[i].fieldSizeBits = dataRef.readUInt16LE();
		fieldInfo[i].additionalDataSize = dataRef.readUInt32LE();
		fieldInfo[i].fieldCompression = dataRef.readUInt32LE();
		auto packVec = dataRef.readUInt32LE(3);
		fieldInfo[i].fieldCompressionPacking.assign(packVec.begin(), packVec.end());
	}

	// char pallet_data[header.pallet_data_size];
	size_t prevPos = dataRef.offset();

	palletData.resize(fieldInfo.size());
	for (size_t fieldIndex = 0; fieldIndex < fieldInfo.size(); fieldIndex++) {
		const auto& thisFieldInfo = fieldInfo[fieldIndex];
		if (thisFieldInfo.fieldCompression == CompressionType::BitpackedIndexed || thisFieldInfo.fieldCompression == CompressionType::BitpackedIndexedArray) {
			palletData[fieldIndex].resize(thisFieldInfo.additionalDataSize / 4);
			for (uint32_t i = 0; i < thisFieldInfo.additionalDataSize / 4; i++)
				palletData[fieldIndex][i] = dataRef.readUInt32LE();
		}
	}

	// Ensure we've read the expected amount of pallet data.
	assert(dataRef.offset() == prevPos + palletDataSize && "Read incorrect amount of pallet data");

	prevPos = dataRef.offset();

	// char common_data[header.common_data_size];
	commonData.resize(fieldInfo.size());
	for (size_t fieldIndex = 0; fieldIndex < fieldInfo.size(); fieldIndex++) {
		const auto& thisFieldInfo = fieldInfo[fieldIndex];
		if (thisFieldInfo.fieldCompression == CompressionType::CommonData) {
			commonData[fieldIndex] = std::unordered_map<uint32_t, uint32_t>{};
			auto& commonDataMap = commonData[fieldIndex].value();

			for (uint32_t i = 0; i < thisFieldInfo.additionalDataSize / 8; i++) {
				uint32_t key = dataRef.readUInt32LE();
				uint32_t val = dataRef.readUInt32LE();
				commonDataMap[key] = val;
			}
		}
	}

	// Ensure we've read the expected amount of common data.
	assert(dataRef.offset() == prevPos + commonDataSize && "Read incorrect amount of common data");

	// New WDC4 chunk: TODO read
	if (wdcVer > 3) {
		for (uint32_t sectionIndex = 0; sectionIndex + 1 < sectionCount; sectionIndex++) {
			uint32_t entryCount = dataRef.readUInt32LE();
			dataRef.move(static_cast<int64_t>(entryCount) * 4);
		}
	}

	// data_sections[header.section_count];
	sections.resize(sectionCount);
	size_t previousStringTableSize = 0;
	for (uint32_t sectionIndex = 0; sectionIndex < sectionCount; sectionIndex++) {
		const auto& hdr = sectionHeaders[sectionIndex];
		const bool isNormal = !(flags & 1);

		const size_t recordDataOfs = dataRef.offset();
		uint32_t recordsOfs;
		uint32_t headerFileOffset;
		if (wdcVer == 2) {
			const auto& hv2 = std::get<WDCSectionHeaderV2>(hdr);
			recordsOfs = hv2.offsetMapOffset;
			headerFileOffset = hv2.fileOffset;
		} else {
			const auto& hv3 = std::get<WDCSectionHeaderV3>(hdr);
			recordsOfs = hv3.offsetRecordsEnd;
			headerFileOffset = hv3.fileOffset;
		}
		const uint32_t headerRecordCount = getSectionRecordCount(hdr);
		const size_t recordDataSize = isNormal ? static_cast<size_t>(recordSize) * headerRecordCount : static_cast<size_t>(recordsOfs) - headerFileOffset;
		const size_t stringBlockOfs = recordDataOfs + recordDataSize;

		WDCSection& sec = sections[sectionIndex];
		sec.header = hdr;
		sec.isNormal = isNormal;
		sec.recordDataOfs = recordDataOfs;
		sec.recordDataSize = recordDataSize;
		sec.stringBlockOfs = stringBlockOfs;

		if (wdcVer == 2 && !isNormal) {
			const auto& hv2 = std::get<WDCSectionHeaderV2>(hdr);
			dataRef.seek(static_cast<int64_t>(hv2.offsetMapOffset));
			// offset_map_entry offset_map[header.max_id - header.min_id + 1];
			const uint32_t offsetMapCount = maxID - minID + 1;
			for (uint32_t i = 0; i < offsetMapCount; i++) {
				OffsetMapEntry entry;
				entry.offset = dataRef.readUInt32LE();
				entry.size = dataRef.readUInt16LE();
				sec.offsetMapByID[minID + i] = entry;
			}
		}

		// store string table offset for lazy access
		sec.stringTableOffset = stringBlockOfs;
		sec.stringTableOffsetBase = previousStringTableSize;

		const uint32_t headerStringTableSize = getSectionStringTableSize(hdr);
		if (wdcVer > 2)
			previousStringTableSize += headerStringTableSize;

		dataRef.seek(static_cast<int64_t>(stringBlockOfs + headerStringTableSize));

		// uint32_t id_list[section_headers.id_list_size / 4];
		const uint32_t idListCount = getSectionIdListSize(hdr) / 4;
		if (idListCount > 0) {
			auto idListRaw = dataRef.readUInt32LE(idListCount);
			sec.idList.assign(idListRaw.begin(), idListRaw.end());
		}

		// copy_table_entry copy_table[section_headers.copy_table_count];
		uint32_t copyTableCount;
		if (wdcVer == 2) {
			const auto& hv2 = std::get<WDCSectionHeaderV2>(hdr);
			copyTableCount = hv2.copyTableSize / 8;
		} else {
			const auto& hv3 = std::get<WDCSectionHeaderV3>(hdr);
			copyTableCount = hv3.copyTableCount;
		}
		for (uint32_t i = 0; i < copyTableCount; i++) {
			int32_t destinationRowID = dataRef.readInt32LE();
			int32_t sourceRowID = dataRef.readInt32LE();
			if (destinationRowID != sourceRowID)
				copyTable[static_cast<uint32_t>(destinationRowID)] = static_cast<uint32_t>(sourceRowID);
		}

		if (wdcVer > 2) {
			const auto& hv3 = std::get<WDCSectionHeaderV3>(hdr);
			// offset_map_entry offset_map[section_headers.offset_map_id_count];
			sec.offsetMapByIndex.resize(hv3.offsetMapIDCount);
			for (uint32_t i = 0; i < hv3.offsetMapIDCount; i++) {
				sec.offsetMapByIndex[i].offset = dataRef.readUInt32LE();
				sec.offsetMapByIndex[i].size = dataRef.readUInt16LE();
			}
		}

		prevPos = dataRef.offset();

		// relationship_map
		const uint32_t relationshipDataSize = getSectionRelationshipDataSize(hdr);
		if (relationshipDataSize > 0) {
			const uint32_t relationshipEntryCount = dataRef.readUInt32LE();
			dataRef.move(8); // relationshipMinID (UInt32) and relationshipMaxID (UInt32)

			for (uint32_t i = 0; i < relationshipEntryCount; i++) {
				const uint32_t foreignID = dataRef.readUInt32LE();
				const uint32_t recordIndex = dataRef.readUInt32LE();
				sec.relationshipMap[recordIndex] = foreignID;

				// populate relationship lookup
				if (relationshipLookup.find(foreignID) == relationshipLookup.end())
					relationshipLookup[foreignID] = std::vector<uint32_t>();
			}

			// If a section is encrypted it is highly likely we don't read the correct amount of data here. Skip ahead if so.
			if (prevPos + relationshipDataSize != dataRef.offset())
				dataRef.seek(static_cast<int64_t>(prevPos + relationshipDataSize));
		}

		// uint32_t offset_map_id_list[section_headers.offset_map_id_count];
		// Duplicate of id_list for sections with offset records.
		if (wdcVer > 2) {
			const auto& hv3 = std::get<WDCSectionHeaderV3>(hdr);
			dataRef.move(static_cast<int64_t>(hv3.offsetMapIDCount) * 4);
		}

		sec.isEncrypted = false;
	}

	// detect encrypted sections and count total records
	totalRecordCount = 0;
	for (uint32_t sectionIndex = 0; sectionIndex < sectionCount; sectionIndex++) {
		auto& section = sections[sectionIndex];
		const uint64_t tactKeyHash = getSectionTactKeyHash(section.header);
		const bool isNormal = section.isNormal;

		// skip parsing entries from encrypted sections
		if (tactKeyHash != 0) {
			bool isZeroed = true;

			// check if record data is all zeroes
			dataRef.seek(static_cast<int64_t>(section.recordDataOfs));
			for (size_t i = 0; i < section.recordDataSize; i++) {
				if (dataRef.readUInt8() != 0x0) {
					isZeroed = false;
					break;
				}
			}

			// check if first integer after string block (from id list or copy table) is non-0
			if (isZeroed && (wdcVer > 2) && isNormal) {
				const auto& hv3 = std::get<WDCSectionHeaderV3>(section.header);
				if (hv3.idListSize > 0 || hv3.copyTableCount > 0) {
					dataRef.seek(static_cast<int64_t>(section.stringBlockOfs + getSectionStringTableSize(section.header)));
					isZeroed = dataRef.readUInt32LE() == 0;
				}
			}

			// check if first entry in offsetMap has size 0
			if (isZeroed && (wdcVer > 2)) {
				const auto& hv3 = std::get<WDCSectionHeaderV3>(section.header);
				if (hv3.offsetMapIDCount > 0)
					isZeroed = section.offsetMapByIndex[0].size == 0;
			}

			if (isZeroed) {
				logging::write("Skipping all-zero encrypted section " + std::to_string(sectionIndex) + " in file " + fileName);
				section.isEncrypted = true;
				continue;
			}
		}

		totalRecordCount += getSectionRecordCount(section.header);
	}

	logging::write("Parsed " + fileName + " with " + std::to_string(size()) + " rows");
	isLoaded = true;
}

/**
 * Lazy-read string from string table by offset.
 * @param stringTableIndex Index into the string table.
 */
std::string WDCReader::_readString(int64_t stringTableIndex) {
	// find which section contains this string table index
	const WDCSection* targetSection = nullptr;
	for (size_t i = 0; i < sections.size(); i++) {
		const auto& sec = sections[i];
		int64_t localOfs = stringTableIndex - static_cast<int64_t>(sec.stringTableOffsetBase);
		if (localOfs >= 0 && localOfs < static_cast<int64_t>(getSectionStringTableSize(sec.header))) {
			targetSection = &sec;
			break;
		}
	}

	if (!targetSection)
		throw std::runtime_error("String table index out of range");

	int64_t localOffset = stringTableIndex - static_cast<int64_t>(targetSection->stringTableOffsetBase);
	data->seek(static_cast<int64_t>(targetSection->stringTableOffset) + localOffset);
	int64_t nullPos = data->indexOf(0x0);
	size_t strLen = static_cast<size_t>(nullPos - static_cast<int64_t>(data->offset()));
	return data->readString(strLen);
}

/**
 * Find which section contains a record ID.
 * @param recordID The record ID to search for.
 */
std::optional<RecordLocation> WDCReader::_findSectionForRecord(uint32_t recordID) {
	for (size_t sectionIndex = 0; sectionIndex < sections.size(); sectionIndex++) {
		const auto& section = sections[sectionIndex];
		if (section.isEncrypted)
			continue;

		const bool hasIDMap = !section.idList.empty();
		const bool emptyIDMap = hasIDMap && allZero(section.idList);

		if (hasIDMap && !emptyIDMap) {
			auto it = std::find(section.idList.begin(), section.idList.end(), recordID);
			if (it != section.idList.end()) {
				size_t recordIndex = static_cast<size_t>(std::distance(section.idList.begin(), it));
				return RecordLocation{ sectionIndex, recordIndex, recordID };
			}
		} else if (emptyIDMap) {
			// for empty id maps, recordID equals recordIndex
			if (recordID < getSectionRecordCount(section.header))
				return RecordLocation{ sectionIndex, static_cast<size_t>(recordID), recordID };
		} else {
			// no id map - need to scan records for inline id field
			uint32_t headerRecordCount = getSectionRecordCount(section.header);
			for (uint32_t recordIndex = 0; recordIndex < headerRecordCount; recordIndex++) {
				auto record = _readRecordFromSection(sectionIndex, recordIndex, 0, false);
				if (record.has_value()) {
					if (!idField.has_value())
						continue;

					auto idIt = record.value().find(*idField);
					if (idIt != record.value().end() && static_cast<uint32_t>(fieldValueToInt64(idIt->second)) == recordID)
						return RecordLocation{ sectionIndex, recordIndex, recordID };
				}
			}
		}
	}

	return std::nullopt;
}

/**
 * Read a record by ID.
 * @param recordID The record ID to read.
 */
std::optional<DataRecord> WDCReader::_readRecord(uint32_t recordID) {
	auto location = _findSectionForRecord(recordID);
	if (!location.has_value())
		return std::nullopt;

	return _readRecordFromSection(location->sectionIndex, location->recordIndex, recordID, true);
}

/**
 * Read a specific record from a section.
 * @param sectionIndex Section index.
 * @param recordIndex Record index within the section.
 * @param recordID Record ID (may be updated during parsing if no ID map).
 * @param hasKnownID Whether the recordID is known upfront.
 */
std::optional<DataRecord> WDCReader::_readRecordFromSection(size_t sectionIndex, size_t recordIndex,
                                                             uint32_t recordID, bool hasKnownID) {
	auto& section = sections[sectionIndex];
	const bool isNormal = section.isNormal;

	if (section.isEncrypted)
		return std::nullopt;

	// total recordDataSize of all forward sections
	size_t outsideDataSize = 0;
	for (size_t i = 0; i < sectionIndex; i++)
		outsideDataSize += sections[i].recordDataSize;

	const bool hasIDMap = !section.idList.empty();
	const bool emptyIDMap = hasIDMap && allZero(section.idList);

	if (hasIDMap && emptyIDMap)
		recordID = static_cast<uint32_t>(recordIndex);

	// for variable-length records, we need recordID to look up offset
	uint32_t recordOfs;
	if (isNormal) {
		recordOfs = static_cast<uint32_t>(recordIndex * recordSize);
	} else {
		if (wdcVersion == 2) {
			auto mapIt = section.offsetMapByID.find(recordID);
			if (mapIt != section.offsetMapByID.end())
				recordOfs = mapIt->second.offset;
			else
				return std::nullopt;
		} else {
			if (recordIndex < section.offsetMapByIndex.size())
				recordOfs = section.offsetMapByIndex[recordIndex].offset;
			else
				return std::nullopt;
		}
	}
	const int64_t absoluteRecordOffs = static_cast<int64_t>(recordOfs) - static_cast<int64_t>(recordCount) * static_cast<int64_t>(recordSize);

	if (!isNormal)
		data->seek(static_cast<int64_t>(recordOfs));
	else
		data->seek(static_cast<int64_t>(section.recordDataOfs + recordOfs));

	DataRecord out;
	size_t fieldIndex = 0;
	for (const auto& prop : schemaOrder) {
		const auto& type = schema.at(prop);

		if (auto* ft = std::get_if<FieldType>(&type)) {
			if (*ft == FieldType::Relation) {
				auto relIt = section.relationshipMap.find(static_cast<uint32_t>(recordIndex));
				if (relIt != section.relationshipMap.end())
					out[prop] = static_cast<int64_t>(relIt->second);
				else
					out[prop] = static_cast<int64_t>(0);

				continue;
			}

			if (*ft == FieldType::NonInlineID) {
				if (hasIDMap)
					out[prop] = static_cast<int64_t>(section.idList[recordIndex]);

				continue;
			}
		}

		const auto& recordFieldInfo = fieldInfo[fieldIndex];

		int count = 0;
		FieldType fieldType;
		if (auto* ft = std::get_if<FieldType>(&type)) {
			fieldType = *ft;
			count = 0;
		} else {
			const auto& arr = std::get<std::pair<FieldType, int>>(type);
			fieldType = arr.first;
			count = arr.second;
		}

		const uint32_t fieldOffsetBytes = recordFieldInfo.fieldOffsetBits / 8;

		switch (recordFieldInfo.fieldCompression) {
			case CompressionType::None:
				switch (fieldType) {
					case FieldType::String:
						if (isNormal) {
							// for WDC3+, strings are in string table
							if (wdcVersion > 2) {
								if (count > 0) {
									std::vector<std::string> strArr(count);
									for (int stringArrayIndex = 0; stringArrayIndex < count; stringArrayIndex++) {
										const int64_t dataPos = static_cast<int64_t>(recordFieldInfo.fieldOffsetBits + (stringArrayIndex * (recordFieldInfo.fieldSizeBits / count))) >> 3;
										const size_t ofsPos = data->offset();
										const uint32_t ofs = data->readUInt32LE();

										if (ofs == 0) {
											strArr[stringArrayIndex] = "";
										} else {
											// string table reference
											const int64_t stringTableIndex = static_cast<int64_t>(outsideDataSize) + absoluteRecordOffs + dataPos + static_cast<int64_t>(ofs);

											if (stringTableIndex == 0)
												strArr[stringArrayIndex] = "";
											else
												strArr[stringArrayIndex] = _readString(stringTableIndex);
										}

										// ensure we're positioned at the next field
										data->seek(static_cast<int64_t>(ofsPos + 4));
									}
									out[prop] = std::move(strArr);
								} else {
									const int64_t dataPos = recordFieldInfo.fieldOffsetBits >> 3;
									const size_t ofsPos = data->offset();
									const uint32_t ofs = data->readUInt32LE();

									if (ofs == 0) {
										out[prop] = std::string("");
									} else {
										// string table reference
										const int64_t stringTableIndex = static_cast<int64_t>(outsideDataSize) + absoluteRecordOffs + dataPos + static_cast<int64_t>(ofs);

										if (stringTableIndex == 0)
											out[prop] = std::string("");
										else
											out[prop] = _readString(stringTableIndex);
									}

									// ensure we're positioned at the next field
									data->seek(static_cast<int64_t>(ofsPos + 4));
								}
							} else {
								// for WDC2, strings are inline in record data
								if (count > 0) {
									std::vector<std::string> strArr(count);
									for (int stringArrayIndex = 0; stringArrayIndex < count; stringArrayIndex++) {
										int64_t nullIdx = data->indexOf(0x0);
										size_t strLen = static_cast<size_t>(nullIdx - static_cast<int64_t>(data->offset()));
										strArr[stringArrayIndex] = data->readString(strLen);
										data->readInt8(); // read NUL character
									}
									out[prop] = std::move(strArr);
								} else {
									int64_t nullIdx = data->indexOf(0x0);
									size_t strLen = static_cast<size_t>(nullIdx - static_cast<int64_t>(data->offset()));
									out[prop] = data->readString(strLen);
									data->readInt8(); // read NUL character
								}
							}
						} else {
							// sparse/offset records always have inline strings
							if (count > 0) {
								std::vector<std::string> strArr(count);
								for (int stringArrayIndex = 0; stringArrayIndex < count; stringArrayIndex++) {
									int64_t nullIdx = data->indexOf(0x0);
									size_t strLen = static_cast<size_t>(nullIdx - static_cast<int64_t>(data->offset()));
									strArr[stringArrayIndex] = data->readString(strLen);
									data->readInt8(); // read NUL character
								}
								out[prop] = std::move(strArr);
							} else {
								int64_t nullIdx = data->indexOf(0x0);
								size_t strLen = static_cast<size_t>(nullIdx - static_cast<int64_t>(data->offset()));
								out[prop] = data->readString(strLen);
								data->readInt8(); // read NUL character
							}
						}
						break;

					case FieldType::Int8:
						if (count > 0) {
							auto vals = data->readInt8(count);
							std::vector<int64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = static_cast<int64_t>(data->readInt8());
						}
						break;
					case FieldType::UInt8:
						if (count > 0) {
							auto vals = data->readUInt8(count);
							std::vector<uint64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = static_cast<uint64_t>(data->readUInt8());
						}
						break;
					case FieldType::Int16:
						if (count > 0) {
							auto vals = data->readInt16LE(count);
							std::vector<int64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = static_cast<int64_t>(data->readInt16LE());
						}
						break;
					case FieldType::UInt16:
						if (count > 0) {
							auto vals = data->readUInt16LE(count);
							std::vector<uint64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = static_cast<uint64_t>(data->readUInt16LE());
						}
						break;
					case FieldType::Int32:
						if (count > 0) {
							auto vals = data->readInt32LE(count);
							std::vector<int64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = static_cast<int64_t>(data->readInt32LE());
						}
						break;
					case FieldType::UInt32:
						if (count > 0) {
							auto vals = data->readUInt32LE(count);
							std::vector<uint64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = static_cast<uint64_t>(data->readUInt32LE());
						}
						break;
					case FieldType::Int64:
						if (count > 0) {
							auto vals = data->readInt64LE(count);
							std::vector<int64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = data->readInt64LE();
						}
						break;
					case FieldType::UInt64:
						if (count > 0) {
							auto vals = data->readUInt64LE(count);
							std::vector<uint64_t> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = data->readUInt64LE();
						}
						break;
					case FieldType::Float:
						if (count > 0) {
							auto vals = data->readFloatLE(count);
							std::vector<float> vec(vals.begin(), vals.end());
							out[prop] = std::move(vec);
						} else {
							out[prop] = data->readFloatLE();
						}
						break;
					default:
						break;
				}
				break;

			case CompressionType::CommonData: {
				auto& cd = commonData[fieldIndex];
				if (cd.has_value()) {
					auto cdIt = cd.value().find(recordID);
					if (cdIt != cd.value().end())
						out[prop] = static_cast<int64_t>(cdIt->second);
					else
						out[prop] = static_cast<int64_t>(recordFieldInfo.fieldCompressionPacking[0]); // default value
				} else {
					out[prop] = static_cast<int64_t>(recordFieldInfo.fieldCompressionPacking[0]);
				}
				break;
			}

			case CompressionType::Bitpacked:
			case CompressionType::BitpackedSigned:
			case CompressionType::BitpackedIndexed:
			case CompressionType::BitpackedIndexedArray: {
				data->seek(static_cast<int64_t>(section.recordDataOfs + recordOfs + fieldOffsetBytes));

				uint64_t rawValue;
				if (data->remainingBytes() >= 8) {
					rawValue = data->readUInt64LE();
				} else {
					castBuffer->seek(0);
					castBuffer->writeBuffer(*data);
					castBuffer->seek(0);
					rawValue = castBuffer->readUInt64LE();
				}

				const uint64_t bitOffset = recordFieldInfo.fieldOffsetBits & 7;
				const uint32_t fieldSizeBits = recordFieldInfo.fieldSizeBits;
				const uint64_t bitMask = fieldSizeBits >= 64
					? std::numeric_limits<uint64_t>::max()
					: ((1ULL << fieldSizeBits) - 1ULL);
				const uint64_t bitpackedValue = (rawValue >> bitOffset) & bitMask;

				if (recordFieldInfo.fieldCompression == CompressionType::BitpackedIndexedArray) {
					uint32_t arrSize = recordFieldInfo.fieldCompressionPacking[2];
					std::vector<uint64_t> arr(arrSize);
					for (uint32_t i = 0; i < arrSize; i++)
						arr[i] = palletData[fieldIndex][static_cast<size_t>(bitpackedValue * arrSize + i)];
					out[prop] = std::move(arr);
				} else if (recordFieldInfo.fieldCompression == CompressionType::BitpackedIndexed) {
					if (bitpackedValue < palletData[fieldIndex].size()) {
						out[prop] = static_cast<uint64_t>(palletData[fieldIndex][static_cast<size_t>(bitpackedValue)]);
					} else {
						logging::write("Skipping record with missing pallet data entry for key " + std::to_string(bitpackedValue) + ", field " + std::to_string(fieldIndex) + " in " + fileName);
						return std::nullopt;
					}
				} else {
					out[prop] = bitpackedValue;
				}

				if (recordFieldInfo.fieldCompression == CompressionType::BitpackedSigned) {
					if (fieldSizeBits == 0) {
						out[prop] = static_cast<int64_t>(0);
					} else if (fieldSizeBits >= 64) {
						out[prop] = static_cast<int64_t>(bitpackedValue);
					} else {
						uint64_t signBit = 1ULL << (fieldSizeBits - 1);
						int64_t signedVal = static_cast<int64_t>((bitpackedValue ^ signBit) - signBit);
						out[prop] = signedVal;
					}
				}

				break;
			}
		}

		// reinterpret field correctly for compression types other than None
		if (recordFieldInfo.fieldCompression != CompressionType::None) {
			bool isArray = std::holds_alternative<std::pair<FieldType, int>>(type);
			if (!isArray) {
				castBuffer->seek(0);
				int64_t rawVal = fieldValueToInt64(out[prop]);
				if (rawVal < 0)
					castBuffer->writeBigInt64LE(rawVal);
				else
					castBuffer->writeBigUInt64LE(static_cast<uint64_t>(rawVal));

				castBuffer->seek(0);
				switch (fieldType) {
					case FieldType::String:
						throw std::runtime_error("Compressed string arrays currently not used/supported.");

					case FieldType::Int8: out[prop] = static_cast<int64_t>(castBuffer->readInt8()); break;
					case FieldType::UInt8: out[prop] = static_cast<uint64_t>(castBuffer->readUInt8()); break;
					case FieldType::Int16: out[prop] = static_cast<int64_t>(castBuffer->readInt16LE()); break;
					case FieldType::UInt16: out[prop] = static_cast<uint64_t>(castBuffer->readUInt16LE()); break;
					case FieldType::Int32: out[prop] = static_cast<int64_t>(castBuffer->readInt32LE()); break;
					case FieldType::UInt32: out[prop] = static_cast<uint64_t>(castBuffer->readUInt32LE()); break;
					case FieldType::Int64: out[prop] = castBuffer->readInt64LE(); break;
					case FieldType::UInt64: out[prop] = castBuffer->readUInt64LE(); break;
					case FieldType::Float: out[prop] = castBuffer->readFloatLE(); break;
					default: break;
				}
			} else {
				uint32_t arrCount = recordFieldInfo.fieldCompressionPacking[2];
				if (auto* uArr = std::get_if<std::vector<uint64_t>>(&out[prop])) {
					switch (fieldType) {
						case FieldType::String:
							throw std::runtime_error("Compressed string arrays currently not used/supported.");

						case FieldType::Float: {
							std::vector<float> result(arrCount);
							for (uint32_t i = 0; i < arrCount && i < uArr->size(); i++) {
								castBuffer->seek(0);
								castBuffer->writeBigUInt64LE((*uArr)[i]);
								castBuffer->seek(0);
								result[i] = castBuffer->readFloatLE();
							}
							out[prop] = std::move(result);
							break;
						}

						case FieldType::Int8:
						case FieldType::Int16:
						case FieldType::Int32:
						case FieldType::Int64: {
							std::vector<int64_t> result(arrCount);
							for (uint32_t i = 0; i < arrCount && i < uArr->size(); i++) {
								castBuffer->seek(0);
								castBuffer->writeBigUInt64LE((*uArr)[i]);
								castBuffer->seek(0);
								switch (fieldType) {
									case FieldType::Int8: result[i] = static_cast<int64_t>(castBuffer->readInt8()); break;
									case FieldType::Int16: result[i] = static_cast<int64_t>(castBuffer->readInt16LE()); break;
									case FieldType::Int32: result[i] = static_cast<int64_t>(castBuffer->readInt32LE()); break;
									case FieldType::Int64: result[i] = castBuffer->readInt64LE(); break;
									default: break;
								}
							}
							out[prop] = std::move(result);
							break;
						}

						default: {
							for (uint32_t i = 0; i < arrCount && i < uArr->size(); i++) {
								castBuffer->seek(0);
								castBuffer->writeBigUInt64LE((*uArr)[i]);
								castBuffer->seek(0);
								switch (fieldType) {
									case FieldType::UInt8: (*uArr)[i] = castBuffer->readUInt8(); break;
									case FieldType::UInt16: (*uArr)[i] = castBuffer->readUInt16LE(); break;
									case FieldType::UInt32: (*uArr)[i] = castBuffer->readUInt32LE(); break;
									case FieldType::UInt64: (*uArr)[i] = castBuffer->readUInt64LE(); break;
									default: break;
								}
							}
							break;
						}
					}
				}
			}
		}

		if (!hasIDMap && fieldIndex == idFieldIndex) {
			recordID = static_cast<uint32_t>(fieldValueToInt64(out[prop]));
			idField = prop;
		}

		fieldIndex++;
	}

	// populate relationship lookup when first reading
	auto relIt = section.relationshipMap.find(static_cast<uint32_t>(recordIndex));
	if (relIt != section.relationshipMap.end()) {
		uint32_t foreignID = relIt->second;
		auto& lookup = relationshipLookup[foreignID];
		if (std::find(lookup.begin(), lookup.end(), recordID) == lookup.end())
			lookup.push_back(recordID);
	}

	return out;
}

std::future<std::optional<DataRecord>> WDCReader::getRowAsync(uint32_t recordID) {
	return std::async(std::launch::async, [this, recordID]() { return getRow(recordID); });
}

std::future<std::optional<DataRecord>> WDCReader::getRowAsync(std::string recordID) {
	return std::async(std::launch::async, [this, recordID = std::move(recordID)]() { return getRow(recordID); });
}

std::future<std::map<uint32_t, DataRecord>> WDCReader::getAllRowsAsync() {
	return std::async(std::launch::async, [this]() { return getAllRows(); });
}

std::future<void> WDCReader::preloadAsync() {
	return std::async(std::launch::async, [this]() { preload(); });
}

std::future<std::vector<DataRecord>> WDCReader::getRelationRowsAsync(uint32_t foreignKeyValue) {
	return std::async(std::launch::async, [this, foreignKeyValue]() { return getRelationRows(foreignKeyValue); });
}

std::future<std::vector<DataRecord>> WDCReader::getRelationRowsAsync(std::string foreignKeyValue) {
	return std::async(std::launch::async, [this, foreignKeyValue = std::move(foreignKeyValue)]() {
		return getRelationRows(foreignKeyValue);
	});
}

std::future<void> WDCReader::loadSchemaAsync(std::string layoutHash) {
	return std::async(std::launch::async, [this, layoutHash = std::move(layoutHash)]() { loadSchema(layoutHash); });
}

std::future<void> WDCReader::parseAsync() {
	return std::async(std::launch::async, [this]() { parse(); });
}

} // namespace db
