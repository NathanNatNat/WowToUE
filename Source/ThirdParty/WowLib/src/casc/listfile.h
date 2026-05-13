/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <regex>
#include <utility>
#include <future>
#include <variant>

namespace casc {

namespace listfile {

struct FilteredEntry {
	uint32_t fileDataID;
	std::string fileName;
};

struct ParsedEntry {
	std::string file_path;
	std::optional<uint32_t> file_data_id;
};

struct ExtFilter {
	std::string ext;
	bool has_exclusion = false;
	std::optional<std::regex> exclusion_regex;

	ExtFilter(const std::string& e) : ext(e), has_exclusion(false) {}
	ExtFilter(const std::string& e, const std::regex& regex)
		: ext(e), has_exclusion(true), exclusion_regex(regex) {}
};

bool preload();
std::shared_future<bool> preloadAsync();

bool prepareListfile();
std::shared_future<bool> prepareListfileAsync();

std::optional<int> applyPreload(const std::unordered_set<uint32_t>& rootEntries);

size_t loadUnknownTextures();
std::future<size_t> loadUnknownTexturesAsync();

size_t loadUnknownModels();
std::future<size_t> loadUnknownModelsAsync();

void loadUnknowns();
std::future<void> loadUnknownsAsync();

bool existsByID(uint32_t id);

std::optional<std::string> getByID(uint32_t id);

std::string getByIDOrUnknown(uint32_t id, const std::string& ext = "");

std::optional<uint32_t> getByFilename(const std::string& filename);

std::vector<std::string> getFilenamesByExtension(const std::vector<ExtFilter>& exts);

std::vector<FilteredEntry> getFilteredEntries(const std::string& search);
std::vector<FilteredEntry> getFilteredEntries(const std::regex& search);

std::vector<std::string> formatEntries(std::vector<uint32_t>& file_data_ids);

std::string formatUnknownFile(uint32_t fileDataID, const std::string& ext = "");

std::string stripFileEntry(const std::string& entry);

ParsedEntry parseFileEntry(const std::string& entry);

void ingestIdentifiedFiles(const std::vector<std::pair<uint32_t, std::string>>& entries);

void addEntry(uint32_t fileDataID, const std::string& fileName,
              std::vector<std::string>* listfile = nullptr);

std::vector<std::string> renderListfile(const std::optional<std::vector<uint32_t>>& file_data_ids = std::nullopt,
                                         bool include_main_index = false);
std::future<std::vector<std::string>> renderListfileAsync(const std::optional<std::vector<uint32_t>>& file_data_ids = std::nullopt,
                                                          bool include_main_index = false);

bool isLoaded();

} // namespace listfile
} // namespace casc
