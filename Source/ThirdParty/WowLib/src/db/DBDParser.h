/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>

class BufferWrapper;

namespace db {

/**
 * Parsed build ID components.
 */
struct BuildID {
	int major = 0;
	int minor = 0;
	int patch = 0;
	int rev   = 0;
};

/**
 * A build range defined by min and max build strings.
 */
struct BuildRange {
	std::string min;
	std::string max;
};

/**
 * Parse a build ID string into components.
 * @param buildID Build ID string (e.g. "1.13.6.36231")
 * @returns BuildID struct
 */
BuildID parseBuildID(const std::string& buildID);

/**
 * Returns true if the provided build falls within the provided range.
 * @param build Build ID string
 * @param min Range minimum build ID string
 * @param max Range maximum build ID string
 * @returns true if build is in range
 */
bool isBuildInRange(const std::string& build, const std::string& min, const std::string& max);

/**
 * Represents a single field in a DBD definition.
 */
class DBDField {
public:
	/**
	 * Construct a new DBDField instance.
	 * @param fieldName Name of the field
	 * @param fieldType Type of the field (column type string)
	 */
	DBDField(const std::string& fieldName, const std::string& fieldType);

	std::string type;
	std::string name;

	bool isSigned    = true;
	bool isID        = false;
	bool isInline    = true;
	bool isRelation  = false;
	int  arrayLength = -1;
	int  size        = -1;
};

/**
 * Represents a single versioned entry in a DBD document.
 */
class DBDEntry {
public:
	/**
	 * Construct a new DBDEntry instance.
	 */
	DBDEntry();

	/**
	 * Add a build to this DBD entry.
	 * @param min Minimum build (or exact build if max is empty)
	 * @param max Maximum build for a range (empty for exact builds)
	 */
	void addBuild(const std::string& min, const std::string& max = "");

	/**
	 * Add layout hashes to this DBD entry.
	 * @param hashes Vector of layout hash strings
	 */
	void addLayoutHashes(const std::vector<std::string>& hashes);

	/**
	 * Add a field to this DBD entry.
	 * @param field The DBDField to add
	 */
	void addField(const DBDField& field);

	/**
	 * Check if this entry is valid for the provided buildID or layout hash.
	 * @param buildID Build ID string
	 * @param layoutHash Layout hash string
	 * @returns true if valid
	 */
	bool isValidFor(const std::string& buildID, const std::optional<std::string>& layoutHash) const;

	std::unordered_set<std::string> builds;
	std::vector<BuildRange> buildRanges;
	std::unordered_set<std::string> layoutHashes;
	std::vector<DBDField> fields;
};

/**
 * Parser for DBD (Database Definition) documents.
 */
class DBDParser {
public:
	/**
	 * Construct a new DBDParser instance.
	 * @param data BufferWrapper containing the DBD document
	 */
	explicit DBDParser(BufferWrapper& data);

	/**
	 * Get a DBD structure for the provided buildID and layoutHash.
	 * @param buildID Build ID string
	 * @param layoutHash Layout hash string
	 * @returns Pointer to matching DBDEntry, or nullptr
	 */
	const DBDEntry* getStructure(const std::string& buildID, const std::optional<std::string>& layoutHash = std::nullopt) const;

	std::vector<DBDEntry> entries;
	std::unordered_map<std::string, std::string> columns;

private:
	/**
	 * Parse the contents of a DBD document.
	 * @param data BufferWrapper containing the DBD document
	 */
	void parse(BufferWrapper& data);

	/**
	 * Parse a chunk from this DBD document.
	 * @param chunk Lines of the chunk
	 */
	void parseChunk(std::vector<std::string>& chunk);

	/**
	 * Parse the column definition of a DBD document.
	 * @param chunk Lines of the column chunk
	 */
	void parseColumnChunk(std::vector<std::string>& chunk);
};

} // namespace db
