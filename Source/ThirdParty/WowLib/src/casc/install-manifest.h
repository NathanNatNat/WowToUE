/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class BufferWrapper;

namespace casc {

/**
 * Represents a tag entry from the install manifest.
 */
struct InstallTag {
	std::string name;
	uint16_t type = 0;
	std::vector<uint8_t> mask;
};

/**
 * Represents a file entry from the install manifest.
 */
struct InstallFile {
	std::string name;
	std::string hash;
	uint32_t size = 0;
	std::vector<std::string> tags;
};

/**
 * Parsed CASC install manifest.
 *
 * JS equivalent: class InstallManifest — module.exports = InstallManifest
 */
class InstallManifest {
public:
	/**
	 * Construct a new InstallManifest instance.
	 * @param data BLTEReader (or BufferWrapper) to parse from.
	 */
	explicit InstallManifest(BufferWrapper& data);

	uint8_t version = 0;
	uint8_t hashSize = 0;
	uint16_t numTags = 0;
	uint32_t numFiles = 0;
	uint32_t maskSize = 0;

	std::vector<InstallTag> tags;
	std::vector<InstallFile> files;

private:
	/**
	 * Parse data for this install manifest.
	 * @param data BLTEReader (or BufferWrapper) to parse from.
	 */
	void parse(BufferWrapper& data);
};

} // namespace casc
