/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <variant>
#include <memory>
#include <future>
#include <string_view>

#include "FieldType.h"
#include "CompressionType.h"

class BufferWrapper;

namespace db {

class DBDEntry;
class DBDField;

/**
 * Schema field value: either a plain FieldType or [FieldType, arrayLength].
 */
using SchemaField = std::variant<FieldType, std::pair<FieldType, int>>;

/**
 * A single parsed record row. Maps field name to a variant value.
 * Scalar values are stored directly; array values as std::vector.
 */
using FieldValue = std::variant<
	int64_t,
	uint64_t,
	float,
	std::string,
	std::vector<int64_t>,
	std::vector<uint64_t>,
	std::vector<float>,
	std::vector<std::string>
>;

using DataRecord = std::map<std::string, FieldValue>;

/**
 * Section header for WDC2.
 */
struct WDCSectionHeaderV2 {
	uint64_t tactKeyHash = 0;
	uint32_t fileOffset = 0;
	uint32_t recordCount = 0;
	uint32_t stringTableSize = 0;
	uint32_t copyTableSize = 0;
	uint32_t offsetMapOffset = 0;
	uint32_t idListSize = 0;
	uint32_t relationshipDataSize = 0;
};

/**
 * Section header for WDC3/WDC4/WDC5.
 */
struct WDCSectionHeaderV3 {
	uint64_t tactKeyHash = 0;
	uint32_t fileOffset = 0;
	uint32_t recordCount = 0;
	uint32_t stringTableSize = 0;
	uint32_t offsetRecordsEnd = 0;
	uint32_t idListSize = 0;
	uint32_t relationshipDataSize = 0;
	uint32_t offsetMapIDCount = 0;
	uint32_t copyTableCount = 0;
};

/**
 * Unified section header (union-like via variant).
 */
using WDCSectionHeader = std::variant<WDCSectionHeaderV2, WDCSectionHeaderV3>;

/**
 * Offset map entry for variable-length records.
 */
struct OffsetMapEntry {
	uint32_t offset = 0;
	uint16_t size = 0;
};

/**
 * Field storage info for a single field.
 */
struct FieldStorageInfo {
	uint16_t fieldOffsetBits = 0;
	uint16_t fieldSizeBits = 0;
	uint32_t additionalDataSize = 0;
	uint32_t fieldCompression = 0;
	std::vector<uint32_t> fieldCompressionPacking;
};

/**
 * Parsed section with all metadata for lazy-loading.
 */
struct WDCSection {
	WDCSectionHeader header;
	bool isNormal = true;
	size_t recordDataOfs = 0;
	size_t recordDataSize = 0;
	size_t stringBlockOfs = 0;
	size_t stringTableOffset = 0;
	size_t stringTableOffsetBase = 0;
	std::vector<uint32_t> idList;
	std::unordered_map<uint32_t, OffsetMapEntry> offsetMapByID;   // WDC2: keyed by recordID
	std::vector<OffsetMapEntry> offsetMapByIndex;                  // WDC3+: keyed by index
	std::unordered_map<uint32_t, uint32_t> relationshipMap;        // recordIndex -> foreignID
	bool isEncrypted = false;
};

/**
 * Result of finding a record in a section.
 */
struct RecordLocation {
	size_t sectionIndex = 0;
	size_t recordIndex = 0;
	uint32_t recordID = 0;
};

/**
 * Returns the schema type for a DBD field.
 * @param entry The DBD field entry.
 * @returns Corresponding FieldType.
 */
FieldType convertDBDToSchemaType(const DBDField& entry);

/**
 * Defines unified logic between WDC2 and WDC3.
 * @class WDCReader
 */
class WDCReader {
public:
	/**
	 * Construct a new WDCReader instance.
	 * @param fileName Name of the DB file.
	 */
	explicit WDCReader(const std::string& fileName);

	/**
	 * Returns the amount of rows available in the table.
	 */
	size_t size() const;

	/**
	 * Get a row from this table.
	 * Returns std::nullopt if the row does not exist.
	 * @param recordID The record ID to look up.
	 */
	std::optional<DataRecord> getRow(uint32_t recordID);
	std::optional<DataRecord> getRow(std::string_view recordID);

	/**
	 * Returns all available rows in the table.
	 * If preload() was called, returns cached rows. Otherwise computes fresh.
	 * Iterates sequentially through all sections for efficient paging with mmap.
	 */
	const std::map<uint32_t, DataRecord>& getAllRows();

	/**
	 * Preload all rows into memory cache.
	 * Subsequent calls to getAllRows() will return cached data.
	 * Required for getRelationRows() to work properly.
	 */
	void preload();

	/**
	 * Get rows by foreign key value (uses relationship maps).
	 * Returns empty vector if no rows found or table has no relationship data.
	 * @param foreignKeyValue The FK value to search for.
	 */
	std::vector<DataRecord> getRelationRows(uint32_t foreignKeyValue);
	std::vector<DataRecord> getRelationRows(std::string_view foreignKeyValue);

	/**
	 * Load the schema for this table.
	 * @param layoutHash The layout hash string.
	 */
	void loadSchema(const std::string& layoutHash);

	/**
	 * Builds a schema for this data table using the provided DBD structure.
	 * @param structure The DBD entry structure.
	 */
	void buildSchemaFromDBDStructure(const DBDEntry& structure);

	/**
	 * Gets index of ID field.
	 */
	uint16_t getIDIndex() const;

	/**
	 * Parse the DB file from CASC.
	 */
	void parse();

	// Async-equivalent API surface mirroring JS Promise methods.
	std::future<std::optional<DataRecord>> getRowAsync(uint32_t recordID);
	std::future<std::optional<DataRecord>> getRowAsync(std::string recordID);
	std::future<std::map<uint32_t, DataRecord>> getAllRowsAsync();
	std::future<void> preloadAsync();
	std::future<std::vector<DataRecord>> getRelationRowsAsync(uint32_t foreignKeyValue);
	std::future<std::vector<DataRecord>> getRelationRowsAsync(std::string foreignKeyValue);
	std::future<void> loadSchemaAsync(std::string layoutHash);
	std::future<void> parseAsync();

	std::string fileName;
	std::map<uint32_t, uint32_t> copyTable;
	std::map<std::string, SchemaField> schema;
	// Ordered list of schema keys to preserve insertion order
	std::vector<std::string> schemaOrder;
	bool isLoaded = false;
	std::optional<std::string> idField;
	uint16_t idFieldIndex = 0;
	std::unordered_map<uint32_t, std::vector<uint32_t>> relationshipLookup;

private:
	/**
	 * Lazy-read string from string table by offset.
	 * @param stringTableIndex Index into the string table.
	 */
	std::string _readString(int64_t stringTableIndex);

	/**
	 * Find which section contains a record ID.
	 * @param recordID The record ID to search for.
	 */
	std::optional<RecordLocation> _findSectionForRecord(uint32_t recordID);

	/**
	 * Read a record by ID.
	 * @param recordID The record ID to read.
	 */
	std::optional<DataRecord> _readRecord(uint32_t recordID);

	/**
	 * Read a specific record from a section.
	 * @param sectionIndex Section index.
	 * @param recordIndex Record index within the section.
	 * @param recordID Record ID (may be updated during parsing if no ID map).
	 * @param hasKnownID Whether the recordID is known upfront.
	 */
	std::optional<DataRecord> _readRecordFromSection(size_t sectionIndex, size_t recordIndex,
	                                                  uint32_t recordID, bool hasKnownID);

	// preloaded rows cache (empty map = not preloaded)
	std::optional<std::map<uint32_t, DataRecord>> rows;
	std::map<uint32_t, DataRecord> transientRows;

	// lazy-loading metadata
	BufferWrapper* data = nullptr;
	std::unique_ptr<BufferWrapper> dataOwner;
	std::vector<WDCSection> sections;
	std::vector<FieldStorageInfo> fieldInfo;
	std::vector<std::vector<uint32_t>> palletData;
	std::vector<std::optional<std::unordered_map<uint32_t, uint32_t>>> commonData;
	std::unique_ptr<BufferWrapper> castBuffer;
	uint32_t recordCount = 0;
	uint32_t recordSize = 0;
	uint16_t flags = 0;
	int wdcVersion = 0;
	uint32_t minID = 0;
	uint32_t maxID = 0;
	uint32_t totalRecordCount = 0;
};

} // namespace db
