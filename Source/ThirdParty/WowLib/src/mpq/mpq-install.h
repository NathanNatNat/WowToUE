/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

#include "mpq.h"

namespace mpq {

/**
 * Listfile entry data — tracks which archive contains a file.
 */
struct ListfileEntry {
	size_t archive_index;
	std::string mpq_name;
	std::string original_filename;
};

/**
 * Archive entry — wraps an MPQArchive with its name.
 */
struct ArchiveEntry {
	std::string name;
	std::unique_ptr<MPQArchive> archive;
};

/**
 * MPQInstall — manages a collection of MPQ archives in a WoW install directory.
 *
 * JS equivalent: class MPQInstall — module.exports = { MPQInstall }
 */
class MPQInstall {
public:
	explicit MPQInstall(const std::string& directory);

	void close();

	/**
	 * Scan and load all MPQ archives in the install directory.
	 * Populates archives, listfile, and detects build version.
	 */
	void loadInstall();

	/**
	 * Get all files matching the given extension.
	 * @param extension File extension to match (e.g. ".blp").
	 * @return List of display paths ("mpq_name\\filename").
	 */
	std::vector<std::string> getFilesByExtension(const std::string& extension) const;

	/**
	 * Get all files in the install (excluding deleted).
	 * @return List of display paths.
	 */
	std::vector<std::string> getAllFiles() const;

	/**
	 * Extract a file by its display path.
	 * @param display_path Path like "patch.mpq\\Creature\\..." or direct internal path.
	 * @return File data, or empty optional if not found.
	 */
	std::optional<std::vector<uint8_t>> getFile(const std::string& display_path) const;

	/**
	 * Get the number of files in the listfile.
	 */
	size_t getFileCount() const;

	/**
	 * Get the number of loaded archives.
	 */
	size_t getArchiveCount() const;

	/// Detected build version string (e.g. "3.3.5.12340").
	std::string build_id;

private:
	std::vector<std::string> _scan_mpq_files(const std::string& dir);

	std::string directory;
	std::vector<ArchiveEntry> archives;
	std::unordered_map<std::string, ListfileEntry> listfile; // lowercase filename -> entry
};

} // namespace mpq
