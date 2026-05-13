/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "DBDParser.h"
#include "../buffer.h"

#include <regex>
#include <stdexcept>
#include <algorithm>
#include <charconv>

namespace db {

/**
 * Pattern to match column definitions in a DBD document.
 */
static const std::regex PATTERN_COLUMN(R"(^(int|float|locstring|string)(<[^:]+::[^>]+>)?\s([^\s]+))");

/**
 * Pattern to match build identifiers in a DBD document.
 */
static const std::regex PATTERN_BUILD(R"(^BUILD\s(.*))");

/**
 * Pattern to match build range identifiers in a DBD document.
 */
static const std::regex PATTERN_BUILD_RANGE(R"(([^-]+)-(.*))");

/**
 * Pattern to match comment entries in a DBD document.
 * Note: Comment data is not captured since it is discarded in parsing.
 */
static const std::regex PATTERN_COMMENT(R"(^COMMENT\s)");

/**
 * Pattern to match layout hash identifiers in a DBD document.
 */
static const std::regex PATTERN_LAYOUT(R"(^LAYOUT\s(.*))");

/**
 * Pattern to match a field entry in a DBD document.
 */
static const std::regex PATTERN_FIELD(R"(^(\$([^$]+)\$)?([^<\[]+)(<(u|)(\d+)>)?(\[(\d+)\])?$)");

/**
 * Pattern to match the components of a build ID.
 */
static const std::regex PATTERN_BUILD_ID(R"((\d+).(\d+).(\d+).(\d+))");

/**
 * Parse a build ID into components.
 * @param buildID Build ID string
 * @returns BuildID struct
 */
BuildID parseBuildID(const std::string& buildID) {
	std::smatch parts;
	BuildID entry;

	if (std::regex_search(buildID, parts, PATTERN_BUILD_ID)) {
		entry.major = std::stoi(parts[1].str());
		entry.minor = std::stoi(parts[2].str());
		entry.patch = std::stoi(parts[3].str());
		entry.rev   = std::stoi(parts[4].str());
	}

	return entry;
}

/**
 * Returns true if the provided build falls within the provided range.
 * @param build Build ID string
 * @param min Range minimum build ID string
 * @param max Range maximum build ID string
 * @returns true if build is in range
 */
bool isBuildInRange(const std::string& build, const std::string& min, const std::string& max) {
	BuildID buildParsed = parseBuildID(build);
	BuildID minParsed   = parseBuildID(min);
	BuildID maxParsed   = parseBuildID(max);

	if (buildParsed.major < minParsed.major || buildParsed.major > maxParsed.major)
		return false;

	if (buildParsed.minor < minParsed.minor || buildParsed.minor > maxParsed.minor)
		return false;

	if (buildParsed.patch < minParsed.patch || buildParsed.patch > maxParsed.patch)
		return false;

	if (buildParsed.rev < minParsed.rev || buildParsed.rev > maxParsed.rev)
		return false;

	return true;
}


DBDField::DBDField(const std::string& fieldName, const std::string& fieldType)
	: type(fieldType)
	, name(fieldName)
	, isSigned(true)
	, isID(false)
	, isInline(true)
	, isRelation(false)
	, arrayLength(-1)
	, size(-1)
{
}


DBDEntry::DBDEntry() = default;

/**
 * Add a build to this DBD entry.
 * @param min Minimum build (or exact build if max is empty)
 * @param max Maximum build for a range (empty for exact builds)
 */
void DBDEntry::addBuild(const std::string& min, const std::string& max) {
	if (!max.empty())
		buildRanges.push_back({ min, max });
	else
		builds.insert(min);
}

/**
 * Add layout hashes to this DBD entry.
 * @param hashes Vector of layout hash strings
 */
void DBDEntry::addLayoutHashes(const std::vector<std::string>& hashes) {
	for (const auto& hash : hashes)
		layoutHashes.insert(hash);
}

/**
 * Add a field to this DBD entry.
 * @param field The DBDField to add
 */
void DBDEntry::addField(const DBDField& field) {
	fields.push_back(field);
}

/**
 * Check if this entry is valid for the provided buildID or layout hash.
 * @param buildID Build ID string
 * @param layoutHash Layout hash string
 * @returns true if valid
 */
bool DBDEntry::isValidFor(const std::string& buildID, const std::optional<std::string>& layoutHash) const {
	// Layout hash takes priority, being the quickest to check.
	if (layoutHash.has_value() && layoutHashes.count(*layoutHash))
		return true;

	// Check for a single build ID.
	if (builds.count(buildID))
		return true;

	// Fallback to checking build ranges.
	for (const auto& range : buildRanges) {
		if (isBuildInRange(buildID, range.min, range.max))
			return true;
	}

	return false;
}


/**
 * Construct a new DBDParser instance.
 * @param data BufferWrapper containing the DBD document
 */
DBDParser::DBDParser(BufferWrapper& data) {
	parse(data);
}

/**
 * Get a DBD structure for the provided buildID and layoutHash.
 * @param buildID Build ID string
 * @param layoutHash Layout hash string
 * @returns Pointer to matching DBDEntry, or nullptr
 */
const DBDEntry* DBDParser::getStructure(const std::string& buildID, const std::optional<std::string>& layoutHash) const {
	for (const auto& entry : entries) {
		if (entry.isValidFor(buildID, layoutHash))
			return &entry;
	}

	return nullptr;
}

/**
 * Parse the contents of a DBD document.
 * @param data BufferWrapper containing the DBD document
 */
void DBDParser::parse(BufferWrapper& data) {
	std::vector<std::string> lines = data.readLines();

	// Separate the file into chunks separated by empty lines.
	std::vector<std::string> chunk;
	for (const auto& line : lines) {
		std::string trimmed = line;
		trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
		trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

		if (!trimmed.empty()) {
			chunk.push_back(line);
		} else {
			parseChunk(chunk);
			chunk.clear();
		}
	}

	// Ensure last chunk is accounted for.
	if (!chunk.empty())
		parseChunk(chunk);

	if (columns.empty())
		throw std::runtime_error("Invalid DBD: No columns defined.");
}

/**
 * Parse a chunk from this DBD document.
 * @param chunk Lines of the chunk
 */
void DBDParser::parseChunk(std::vector<std::string>& chunk) {
	if (!chunk.empty() && chunk[0] == "COLUMNS") {
		parseColumnChunk(chunk);
	} else {
		DBDEntry entry;
		for (const auto& line : chunk) {
			// Build IDs.
			std::smatch buildMatch;
			if (std::regex_search(line, buildMatch, PATTERN_BUILD)) {
				// BUILD 1.7.0.4671-1.8.0.4714
				// BUILD 0.9.1.3810
				// BUILD 1.13.6.36231, 1.13.6.36310
				const std::string& buildList = buildMatch[1].str();

				std::vector<std::string> builds;
				size_t start = 0;
				size_t pos = 0;
				while ((pos = buildList.find(',', start)) != std::string::npos) {
					builds.push_back(buildList.substr(start, pos - start));
					start = pos + 1;
				}
				builds.push_back(buildList.substr(start));

				for (const auto& build : builds) {
					std::smatch buildRange;
					std::string trimmedBuild = build;
					trimmedBuild.erase(0, trimmedBuild.find_first_not_of(" \t"));
					trimmedBuild.erase(trimmedBuild.find_last_not_of(" \t") + 1);

					if (std::regex_search(trimmedBuild, buildRange, PATTERN_BUILD_RANGE))
						entry.addBuild(buildRange[1].str(), buildRange[2].str());
					else
						entry.addBuild(trimmedBuild);
				}

				continue;
			}

			// Skip comments.
			if (std::regex_search(line, PATTERN_COMMENT))
				continue;

			// Layout hashes.
			std::smatch layoutMatch;
			if (std::regex_search(line, layoutMatch, PATTERN_LAYOUT)) {
				// LAYOUT 0E84A21C, 35353535
				const std::string& hashList = layoutMatch[1].str();

				std::vector<std::string> hashes;
				size_t start = 0;
				size_t pos = 0;
				while ((pos = hashList.find(',', start)) != std::string::npos) {
					std::string h = hashList.substr(start, pos - start);
					h.erase(0, h.find_first_not_of(" \t"));
					h.erase(h.find_last_not_of(" \t") + 1);
					hashes.push_back(h);
					start = pos + 1;
				}
				std::string h = hashList.substr(start);
				h.erase(0, h.find_first_not_of(" \t"));
				h.erase(h.find_last_not_of(" \t") + 1);
				hashes.push_back(h);

				entry.addLayoutHashes(hashes);
				continue;
			}

			std::smatch fieldMatch;
			if (std::regex_search(line, fieldMatch, PATTERN_FIELD)) {
				const std::string fieldName = fieldMatch[3].str();
				auto it = columns.find(fieldName);

				if (it == columns.end())
					throw std::runtime_error("Invalid DBD: No field type defined for " + fieldName);

				const std::string& fieldType = it->second;
				DBDField field(fieldName, fieldType);

				// Parse annotations, (eg 'id,noninline,relation').
				if (fieldMatch[2].matched) {
					const std::string annotationsStr = fieldMatch[2].str();
					std::vector<std::string> annotations;
					size_t aStart = 0;
					size_t aPos = 0;
					while ((aPos = annotationsStr.find(',', aStart)) != std::string::npos) {
						annotations.push_back(annotationsStr.substr(aStart, aPos - aStart));
						aStart = aPos + 1;
					}
					annotations.push_back(annotationsStr.substr(aStart));

					auto contains = [&](const std::string& needle) {
						return std::find(annotations.begin(), annotations.end(), needle) != annotations.end();
					};

					if (contains("id"))
						field.isID = true;

					if (contains("noninline"))
						field.isInline = false;

					if (contains("relation"))
						field.isRelation = true;
				}

				// Parse signedness, either 'u' or undefined.
				if (fieldMatch[5].matched && !fieldMatch[5].str().empty())
					field.isSigned = false;

				// Parse data size (eg '32').
				if (fieldMatch[6].matched) {
					try {
						int dataSize = std::stoi(fieldMatch[6].str());
						field.size = dataSize;
					} catch (...) {
					}
				}

				// Parse array size (eg '2').
				if (fieldMatch[8].matched) {
					try {
						int arrayLength = std::stoi(fieldMatch[8].str());
						field.arrayLength = arrayLength;
					} catch (...) {
					}
				}

				entry.addField(field);
			}
		}

		entries.push_back(std::move(entry));
	}
}

/**
 * Parse the column definition of a DBD document.
 * @param chunk Lines of the column chunk
 */
void DBDParser::parseColumnChunk(std::vector<std::string>& chunk) {
	if (chunk.empty())
		throw std::runtime_error("Invalid DBD: Missing column definitions.");

	// Remove the COLUMNS header.
	chunk.erase(chunk.begin());

	for (const auto& entry : chunk) {
		std::smatch match;
		if (std::regex_search(entry, match, PATTERN_COLUMN)) {
			const std::string columnType = match[1].str(); // int|float|locstring|string
			//const std::string columnForeignKey = match[2].str(); // <TableName::ColumnName> or undefined
			std::string columnName = match[3].str(); // Field_6_0_1_18179_000?

			const size_t qpos = columnName.find('?');
			if (qpos != std::string::npos)
				columnName.erase(qpos, 1);

			// TODO: Support foreign key support.

			columns[columnName] = columnType;
		}
	}
}

} // namespace db
