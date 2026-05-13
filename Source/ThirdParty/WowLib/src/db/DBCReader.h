/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <optional>

#include "FieldType.h"
#include "WDCReader.h"

class BufferWrapper;

namespace db {

class DBDEntry;

// locale count changed over time:
// vanilla/tbc (pre-wotlk): 8 locales
// wotlk onwards: 16 locales
inline constexpr int LOCALE_COUNT_PRE_WOTLK = 8;
inline constexpr int LOCALE_COUNT_WOTLK = 16;

/**
 * Returns the schema type symbol for a DBD field.
 * @param entry
 * @returns FieldType
 */
FieldType convert_dbd_to_schema_type(const DBDField& entry);

/**
 * Schema field info for DBC reader.
 */
struct DBCSchemaField {
	FieldType type = FieldType::UInt32;
	bool is_locstring = false;
	int array_length = -1;
};

/**
 * DBC file reader for legacy WoW clients (pre-Cataclysm).
 * @class DBCReader
 */
class DBCReader {
public:
	/**
	 * Construct a new DBCReader instance.
	 * @param file_name
	 * @param build_id - Build version string (e.g., '1.12.1.5875')
	 */
	DBCReader(const std::string& file_name, const std::string& build_id);

	/**
	 * Returns the amount of rows available in the table.
	 */
	size_t size() const;

	/**
	 * Get a row from this table by index.
	 * @param index
	 */
	std::optional<DataRecord> getRow(int index);

	/**
	 * Returns all available rows in the table.
	 */
	std::map<uint32_t, DataRecord> getAllRows();

	/**
	 * Preload all rows into memory cache.
	 */
	void preload();

	/**
	 * Load the schema for this table from DBD definitions.
	 */
	void loadSchema();

	/**
	 * Parse the DBC file from raw data.
	 * @param data
	 */
	void parse(BufferWrapper& data);

	std::string file_name;
	std::string build_id;

	std::map<std::string, DBCSchemaField> schema;
	// Ordered list of schema keys to preserve insertion order
	std::vector<std::string> schemaOrder;
	bool is_loaded = false;

	uint32_t record_count = 0;
	uint32_t field_count = 0;
	uint32_t record_size = 0;
	uint32_t string_block_size = 0;

	int locale_count = LOCALE_COUNT_PRE_WOTLK;

private:
	/**
	 * Determine locale count based on build version.
	 * @param build_id
	 * @returns locale count
	 */
	int _get_locale_count(const std::string& build_id);

	/**
	 * Builds a schema from DBD structure.
	 * @param structure
	 */
	void _build_schema_from_dbd(const DBDEntry& structure);

	/**
	 * Builds a fallback schema when no DBD is available.
	 */
	void _build_fallback_schema();

	/**
	 * Calculate total field count from schema.
	 * @returns field count
	 */
	int _calculate_schema_field_count();

	/**
	 * Read a string from the string block.
	 * @param offset
	 * @returns string
	 */
	std::string _read_string(uint32_t offset);

	/**
	 * Read a record by index.
	 * @param index
	 * @returns record or std::nullopt
	 */
	std::optional<DataRecord> _read_record(int index);

	/**
	 * Read a single field value.
	 * @param field_type
	 * @returns FieldValue
	 */
	FieldValue _read_field(FieldType field_type);

	/**
	 * Read an array of field values.
	 * @param field_type
	 * @param count
	 * @returns vector of FieldValues
	 */
	std::vector<FieldValue> _read_field_array(FieldType field_type, int count);

	std::optional<std::map<uint32_t, DataRecord>> rows;
	BufferWrapper* data = nullptr;
	BufferWrapper* string_block = nullptr;
	uint32_t string_block_offset = 0;
};

} // namespace db
