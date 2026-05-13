/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "mpq-install.h"
#include "build-version.h"

#include "../log.h"
#include "../core.h"

#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <format>

namespace mpq {

namespace fs = std::filesystem;

MPQInstall::MPQInstall(const std::string& directory)
	: directory(directory) {}

void MPQInstall::close() {
	for (auto& entry : archives)
		entry.archive->close();
}

std::vector<std::string> MPQInstall::_scan_mpq_files(const std::string& dir) {
	std::vector<std::string> results;

	for (const auto& entry : fs::directory_iterator(dir)) {
		const auto full_path = entry.path().string();

		if (entry.is_directory()) {
			auto sub_results = _scan_mpq_files(full_path);
			results.insert(results.end(), sub_results.begin(), sub_results.end());
		} else {
			std::string filename = entry.path().filename().string();
			std::transform(filename.begin(), filename.end(), filename.begin(),
			               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".mpq")
				results.push_back(full_path);
		}
	}

	std::sort(results.begin(), results.end());
	return results;
}

void MPQInstall::loadInstall() {
	core::progressLoadingScreen("Scanning for MPQ Archives");

	std::vector<std::string> mpq_files = _scan_mpq_files(directory);

	if (mpq_files.empty())
		throw std::runtime_error("No MPQ archives found in directory");

	logging::write(std::format("Found {} MPQ archives in {}", mpq_files.size(), directory));

	core::progressLoadingScreen("Loading MPQ Archives");

	for (const auto& mpq_path : mpq_files) {
		auto archive = std::make_unique<MPQArchive>(mpq_path);
		const auto info = archive->getInfo();
		const auto mpq_name = fs::relative(fs::path(mpq_path), fs::path(directory)).string();

		logging::write(std::format("Loaded {}: format v{}, {} files, {} hash entries, {} block entries",
			mpq_name, info.formatVersion, info.fileCount, info.hashTableEntries, info.blockTableEntries));

		archives.push_back(ArchiveEntry{
			mpq_name,
			std::move(archive),
		});

		for (const auto& filename : archives.back().archive->files) {
			std::string lower_filename = filename;
			std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(),
			               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			listfile[lower_filename] = ListfileEntry{
				archives.size() - 1,
				mpq_name,
				filename
			};
		}
	}

	core::progressLoadingScreen("MPQ Archives Loaded");
	logging::write(std::format("Total files in listfile: {}", listfile.size()));

	// detect build version
	std::vector<std::string> mpq_names;
	mpq_names.reserve(archives.size());
	for (const auto& a : archives)
		mpq_names.push_back(a.name);

	build_id = detect_build_version(directory, mpq_names);
	logging::write(std::format("Using build version: {}", build_id));
}

std::vector<std::string> MPQInstall::getFilesByExtension(const std::string& extension) const {
	std::string ext = extension;
	std::transform(ext.begin(), ext.end(), ext.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	std::vector<std::string> results;

	for (const auto& [filename, data] : listfile) {
		if (filename.size() >= ext.size() &&
		    filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
			results.push_back(data.mpq_name + "\\" + filename);
		}
	}

	return results;
}

std::vector<std::string> MPQInstall::getAllFiles() const {
	std::vector<std::string> results;

	for (const auto& [filename, data] : listfile) {
		const auto& archive = archives[data.archive_index].archive;
		const auto* hash_entry = archive->getHashTableEntry(data.original_filename);

		if (hash_entry) {
			if (hash_entry->blockTableIndex < archive->blockTable.size()) {
				const auto& block_entry = archive->blockTable[hash_entry->blockTableIndex];

				// Skip files marked as deleted
				if (block_entry.flags & MPQ_FILE_DELETE_MARKER)
					continue;
			}
		}

		results.push_back(data.mpq_name + "\\" + filename);
	}

	return results;
}

std::optional<std::vector<uint8_t>> MPQInstall::getFile(const std::string& display_path) const {
	// normalize path separators (some files use forward slashes)
	std::string normalized_path = display_path;
	std::replace(normalized_path.begin(), normalized_path.end(), '/', '\\');

	// first try direct lookup (for paths without mpq prefix like texture references)
	std::string direct_normalized = normalized_path;
	std::transform(direct_normalized.begin(), direct_normalized.end(), direct_normalized.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	auto direct_it = listfile.find(direct_normalized);
	if (direct_it != listfile.end()) {
		const auto& data = direct_it->second;
		const auto& archive = archives[data.archive_index].archive;
		return archive->extractFile(data.original_filename);
	}

	// try stripping mpq name prefix (for display paths like "patch.mpq\Creature\...")
	if (normalized_path.find('\\') != std::string::npos) {
		std::vector<std::string> parts;
		size_t start = 0;
		while (start < normalized_path.size()) {
			size_t pos = normalized_path.find('\\', start);
			if (pos == std::string::npos) {
				parts.push_back(normalized_path.substr(start));
				break;
			}
			parts.push_back(normalized_path.substr(start, pos - start));
			start = pos + 1;
		}

		// try progressively stripping path components
		for (size_t i = 1; i < parts.size(); i++) {
			std::string test_filename;
			for (size_t j = i; j < parts.size(); j++) {
				if (!test_filename.empty())
					test_filename += '\\';
				test_filename += parts[j];
			}

			std::transform(test_filename.begin(), test_filename.end(), test_filename.begin(),
			               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			auto test_it = listfile.find(test_filename);
			if (test_it != listfile.end()) {
				const auto& data = test_it->second;
				const auto& archive = archives[data.archive_index].archive;
				return archive->extractFile(data.original_filename);
			}
		}
	}

	return std::nullopt;
}

size_t MPQInstall::getFileCount() const {
	return listfile.size();
}

size_t MPQInstall::getArchiveCount() const {
	return archives.size();
}

} // namespace mpq
