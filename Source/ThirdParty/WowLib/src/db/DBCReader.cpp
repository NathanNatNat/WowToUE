/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "DBCReader.h"
#include "DBDParser.h"
#include "FieldType.h"

#include "../log.h"
#include "../core.h"
#include "../generics.h"
#include "../constants.h"
#include "../buffer.h"
#include "../casc/export-helper.h"
#include "../casc/dbd-manifest.h"
#include "../casc/casc-source.h"
#include "../casc/casc-source-local.h"
#include "../casc/casc-source-remote.h"
#include "../casc/build-cache.h"

#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <memory>

namespace db {

static constexpr uint32_t DBC_MAGIC = 0x43424457; // 'WDBC'

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
 * Returns the schema type symbol for a DBD field.
 * @param entry
 * @returns FieldType
 */
FieldType convert_dbd_to_schema_type(const DBDField& entry) {
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
			default: return entry.isSigned ? FieldType::Int32 : FieldType::UInt32;
		}
	}

	return FieldType::UInt32;
}

/**
 * Construct a new DBCReader instance.
 * @param file_name
 * @param build_id - Build version string (e.g., '1.12.1.5875')
 */
DBCReader::DBCReader(const std::string& file_name, const std::string& build_id)
	: file_name(file_name)
	, build_id(build_id)
{
	// determine locale count based on build version
	// wotlk (3.x) introduced 16 locales, earlier versions use 8
	locale_count = _get_locale_count(build_id);
}

/**
 * Determine locale count based on build version.
 * @param build_id
 * @returns locale count
 */
int DBCReader::_get_locale_count(const std::string& build_id) {
	std::regex pattern(R"((\d+)\.(\d+)\.(\d+)\.(\d+))");
	std::smatch parts;
	if (!std::regex_search(build_id, parts, pattern))
		return LOCALE_COUNT_PRE_WOTLK;

	const int major = std::stoi(parts[1].str());

	// wotlk is 3.x, tbc is 2.x, vanilla is 1.x
	if (major >= 3)
		return LOCALE_COUNT_WOTLK;

	return LOCALE_COUNT_PRE_WOTLK;
}

/**
 * Returns the amount of rows available in the table.
 */
size_t DBCReader::size() const {
	return static_cast<size_t>(record_count);
}

/**
 * Get a row from this table by index.
 * @param index
 */
std::optional<DataRecord> DBCReader::getRow(int index) {
	if (!is_loaded)
		throw std::runtime_error("Attempted to read a data table row before table was loaded.");

	if (rows.has_value())
		return rows.value().count(static_cast<uint32_t>(index)) ? std::optional<DataRecord>(rows.value().at(static_cast<uint32_t>(index))) : std::nullopt;

	return _read_record(index);
}

/**
 * Returns all available rows in the table.
 */
std::map<uint32_t, DataRecord> DBCReader::getAllRows() {
	if (!is_loaded)
		throw std::runtime_error("Attempted to read a data table rows before table was loaded.");

	if (rows.has_value())
		return rows.value();

	std::map<uint32_t, DataRecord> result;
	for (uint32_t i = 0; i < record_count; i++) {
		auto record = _read_record(static_cast<int>(i));
		if (record.has_value()) {
			uint32_t id = i;
			auto idIt = record.value().find("ID");
			if (idIt != record.value().end()) {
				if (auto* p = std::get_if<int64_t>(&idIt->second))
					id = static_cast<uint32_t>(*p);
				else if (auto* p = std::get_if<uint64_t>(&idIt->second))
					id = static_cast<uint32_t>(*p);
			}
			result[id] = std::move(record.value());
		}
	}

	return result;
}

/**
 * Preload all rows into memory cache.
 */
void DBCReader::preload() {
	if (!is_loaded)
		throw std::runtime_error("Attempted to preload table before it was loaded.");

	if (rows.has_value())
		return;

	rows = getAllRows();
}

/**
 * Load the schema for this table from DBD definitions.
 */
void DBCReader::loadSchema() {
	std::filesystem::path fileBaseName = std::filesystem::path(file_name).filename();
	const std::string raw_table_name = casc::ExportHelper::replaceExtension(fileBaseName.string());

	// resolve proper casing from dbd manifest
	casc::dbd_manifest::prepareManifest();
	const auto all_tables = casc::dbd_manifest::getAllTableNames();
	std::string table_name_lower = raw_table_name;
	std::transform(table_name_lower.begin(), table_name_lower.end(), table_name_lower.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	std::string table_name = raw_table_name;
	for (const auto& t : all_tables) {
		std::string t_lower = t;
		std::transform(t_lower.begin(), t_lower.end(), t_lower.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (t_lower == table_name_lower) {
			table_name = t;
			break;
		}
	}

	const std::string dbd_name = table_name + ".dbd";

	const DBDEntry* structure = nullptr;
	logging::write("Loading table definitions " + dbd_name + " (" + build_id + ")...");

	// check cached dbd (use lowercase for cache key consistency)
	std::string cache_key = dbd_name;
	std::transform(cache_key.begin(), cache_key.end(), cache_key.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	std::unique_ptr<DBDParser> dbdParser;
	casc::BuildCache* cache = getActiveCascCache();

	if (cache != nullptr) {
		auto cachedRaw = cache->getFile(cache_key, constants::CACHE::DIR_DBD().string());
		if (cachedRaw.has_value()) {
			dbdParser = std::make_unique<DBDParser>(cachedRaw.value());
			structure = dbdParser->getStructure(build_id);
		}
	}

	// download if not cached or structure not found
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

		const std::string dbd_url = formatURL(configDbdURL, table_name);
		const std::string dbd_url_fallback = formatURL(configDbdFallbackURL, table_name);

		try {
			logging::write("No cached DBD or no matching structure, downloading from " + dbd_url);
			BufferWrapper raw_dbd = generics::downloadFile({ dbd_url, dbd_url_fallback });

			if (cache != nullptr) {
				cache->storeFile(cache_key, raw_dbd, constants::CACHE::DIR_DBD().string());
			}

			dbdParser = std::make_unique<DBDParser>(raw_dbd);
			structure = dbdParser->getStructure(build_id);
		} catch (const std::exception& e) {
			logging::write(std::string("Failed to download DBD for ") + table_name + ": " + e.what());
		}
	}

	if (structure == nullptr) {
		logging::write("No table definition available for " + table_name + ", using raw field names");
		_build_fallback_schema();
		return;
	}

	_build_schema_from_dbd(*structure);
}

/**
 * Builds a schema from DBD structure.
 * @param structure
 */
void DBCReader::_build_schema_from_dbd(const DBDEntry& structure) {
	for (const auto& field : structure.fields) {
		const FieldType field_type = convert_dbd_to_schema_type(field);

		// locstring fields in pre-cata have 16 locale columns + 1 bitmask
		if (field.type == "locstring") {
			schema[field.name] = { field_type, true, field.arrayLength };
			schemaOrder.push_back(field.name);
		} else if (field.arrayLength > -1) {
			schema[field.name] = { field_type, false, field.arrayLength };
			schemaOrder.push_back(field.name);
		} else {
			schema[field.name] = { field_type, false, -1 };
			schemaOrder.push_back(field.name);
		}
	}
}

/**
 * Builds a fallback schema when no DBD is available.
 */
void DBCReader::_build_fallback_schema() {
	for (uint32_t i = 0; i < field_count; i++) {
		const std::string name = (i == 0) ? "ID" : ("field_" + std::to_string(i));
		schema[name] = { FieldType::UInt32, false, -1 };
		schemaOrder.push_back(name);
	}
}

/**
 * Parse the DBC file from raw data.
 * @param data
 */
void DBCReader::parse(BufferWrapper& data) {
	logging::write("Loading DBC file " + file_name);

	this->data = &data;

	// read header
	const uint32_t magic = data.readUInt32LE();
	if (magic != DBC_MAGIC) {
		std::ostringstream oss;
		oss << std::hex << magic;
		throw std::runtime_error("Invalid DBC magic: " + oss.str());
	}

	record_count = data.readUInt32LE();
	field_count = data.readUInt32LE();
	record_size = data.readUInt32LE();
	string_block_size = data.readUInt32LE();

	// calculate offsets
	const uint32_t records_offset = 20; // header is always 20 bytes
	string_block_offset = records_offset + (record_count * record_size);

	// load schema
	loadSchema();

	// validate schema matches actual dbc field count
	const int schema_field_count = _calculate_schema_field_count();
	if (schema_field_count != static_cast<int>(field_count)) {
		logging::write("Schema mismatch for " + file_name + ": schema has " + std::to_string(schema_field_count) +
			" fields, DBC has " + std::to_string(field_count) + " fields. Using fallback.");
		schema.clear();
		schemaOrder.clear();
		_build_fallback_schema();
	}

	logging::write("Parsed DBC " + file_name + " with " + std::to_string(record_count) +
		" rows, " + std::to_string(field_count) + " fields, " + std::to_string(record_size) + " bytes per record");

	is_loaded = true;
}

/**
 * Calculate total field count from schema.
 * @returns field count
 */
int DBCReader::_calculate_schema_field_count() {
	int count = 0;
	for (const auto& name : schemaOrder) {
		const auto& field_info = schema.at(name);
		if (field_info.is_locstring) {
			// locstring: locale_count + 1 bitmask
			const int array_len = field_info.array_length > 0 ? field_info.array_length : 1;
			count += (locale_count + 1) * array_len;
		} else if (field_info.array_length > 0) {
			count += field_info.array_length;
		} else {
			count += 1;
		}
	}
	return count;
}

/**
 * Read a string from the string block.
 * @param offset
 * @returns string
 */
std::string DBCReader::_read_string(uint32_t offset) {
	if (offset == 0)
		return "";

	const size_t abs_offset = string_block_offset + offset;
	data->seek(static_cast<int64_t>(abs_offset));

	const int64_t end = data->indexOf(0x0);
	if (end == -1)
		return "";

	const size_t length = static_cast<size_t>(end) - abs_offset;
	data->seek(static_cast<int64_t>(abs_offset));
	return data->readString(length);
}

/**
 * Read a record by index.
 * @param index
 * @returns record or std::nullopt
 */
std::optional<DataRecord> DBCReader::_read_record(int index) {
	if (index < 0 || index >= static_cast<int>(record_count))
		return std::nullopt;

	const size_t record_offset = 20 + (static_cast<size_t>(index) * record_size);
	data->seek(static_cast<int64_t>(record_offset));

	DataRecord out;
	int field_index = 0;

	for (const auto& name : schemaOrder) {
		const auto& field_info = schema.at(name);
		const FieldType field_type = field_info.type;
		const bool is_locstring = field_info.is_locstring;
		const int array_length = field_info.array_length;

		if (is_locstring) {
			// locstring: N locale offsets + 1 bitmask
			// vanilla/tbc: 8 locales + 1 bitmask = 9 fields
			// wotlk+: 16 locales + 1 bitmask = 17 fields
			const int locstring_field_count = locale_count + 1;
			const auto locale_offsets = data->readUInt32LE(static_cast<size_t>(locale_count));
			data->readUInt32LE(); // bitmask, skip

			// use first non-empty locale (usually enUS at index 0)
			std::string value;
			for (int i = 0; i < locale_count; i++) {
				const std::string str = _read_string(locale_offsets[static_cast<size_t>(i)]);
				if (!str.empty()) {
					value = str;
					break;
				}
			}

			// restore position after reading string
			const size_t next_offset = record_offset + (static_cast<size_t>(field_index + locstring_field_count) * 4);
			data->seek(static_cast<int64_t>(next_offset));

			if (array_length > -1) {
				// if it's an array of locstrings, only store single for now
				out[name] = value;
			} else {
				out[name] = value;
			}

			field_index += locstring_field_count;
		} else if (array_length > -1) {
			auto values = _read_field_array(field_type, array_length);
			if (!values.empty()) {
				if (std::holds_alternative<std::string>(values[0])) {
					std::vector<std::string> arr;
					arr.reserve(values.size());
					for (auto& v : values)
						arr.push_back(std::get<std::string>(v));
					out[name] = std::move(arr);
				} else if (std::holds_alternative<float>(values[0])) {
					std::vector<float> arr;
					arr.reserve(values.size());
					for (auto& v : values)
						arr.push_back(std::get<float>(v));
					out[name] = std::move(arr);
				} else if (std::holds_alternative<int64_t>(values[0])) {
					std::vector<int64_t> arr;
					arr.reserve(values.size());
					for (auto& v : values)
						arr.push_back(std::get<int64_t>(v));
					out[name] = std::move(arr);
				} else if (std::holds_alternative<uint64_t>(values[0])) {
					std::vector<uint64_t> arr;
					arr.reserve(values.size());
					for (auto& v : values)
						arr.push_back(std::get<uint64_t>(v));
					out[name] = std::move(arr);
				} else {
					out[name] = std::vector<int64_t>{};
				}
			} else {
				out[name] = std::vector<int64_t>{};
			}
			field_index += array_length;
		} else {
			out[name] = _read_field(field_type);
			field_index++;
		}
	}

	return out;
}

/**
 * Read a single field value.
 * @param field_type
 * @returns FieldValue
 */
FieldValue DBCReader::_read_field(FieldType field_type) {
	switch (field_type) {
		case FieldType::String: {
			const uint32_t offset = data->readUInt32LE();
			const size_t pos = data->offset();
			const std::string str = _read_string(offset);
			data->seek(static_cast<int64_t>(pos));
			return str;
		}

		case FieldType::Int8: return static_cast<int64_t>(data->readInt8());
		case FieldType::UInt8: return static_cast<uint64_t>(data->readUInt8());
		case FieldType::Int16: return static_cast<int64_t>(data->readInt16LE());
		case FieldType::UInt16: return static_cast<uint64_t>(data->readUInt16LE());
		case FieldType::Int32: return static_cast<int64_t>(data->readInt32LE());
		case FieldType::UInt32: return static_cast<uint64_t>(data->readUInt32LE());
		case FieldType::Float: return data->readFloatLE();

		default:
			return static_cast<uint64_t>(data->readUInt32LE());
	}
}

/**
 * Read an array of field values.
 * @param field_type
 * @param count
 * @returns vector of FieldValues
 */
std::vector<FieldValue> DBCReader::_read_field_array(FieldType field_type, int count) {
	std::vector<FieldValue> values(static_cast<size_t>(count));
	for (int i = 0; i < count; i++)
		values[static_cast<size_t>(i)] = _read_field(field_type);

	return values;
}

} // namespace db
