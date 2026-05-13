/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
*/
#include "listfile.h"
#include "export-helper.h"
#include "../generics.h"
#include "../constants.h"
#include "../core.h"
#include "../log.h"
#include "../buffer.h"

#include "../db/caches/DBTextureFileData.h"
#include "../db/caches/DBModelFileData.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <regex>
#include <chrono>
#include <cstring>
#include <format>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace casc {
namespace listfile {

static std::unordered_map<std::string, uint32_t> legacy_name_lookup;
static std::unordered_map<uint32_t, std::string> legacy_id_lookup;
static std::shared_mutex legacy_data_mutex;

static bool loaded = false;

static std::unordered_map<uint32_t, std::string> preloadedIdLookup;
static std::unordered_map<std::string, uint32_t> preloadedNameLookup;

static std::vector<uint32_t> preload_textures_ids;
static std::vector<uint32_t> preload_sounds_ids;
static std::vector<uint32_t> preload_text_ids;
static std::vector<uint32_t> preload_fonts_ids;
static std::vector<uint32_t> preload_models_ids;

static bool is_preloaded = false;
static std::optional<std::shared_future<bool>> preload_future;
static std::mutex preload_mutex;

static std::vector<uint32_t> getFileDataIDsByExtension(const std::vector<ExtFilter>& exts, std::string_view name) {
	std::vector<uint32_t> entries;

	std::vector<std::pair<uint32_t, std::string>> entriesArray(preloadedIdLookup.begin(), preloadedIdLookup.end());

	generics::batchWork<std::pair<uint32_t, std::string>>(name, entriesArray,
		[&](const std::pair<uint32_t, std::string>& item, size_t) {
			const auto& [fileDataID, filename] = item;
			for (const auto& ext : exts) {
				if (ext.has_exclusion && ext.exclusion_regex) {
					if (filename.ends_with(ext.ext) && !std::regex_search(filename, *ext.exclusion_regex)) {
						entries.push_back(fileDataID);
						break;
					}
				} else {
					if (filename.ends_with(ext.ext)) {
						entries.push_back(fileDataID);
						break;
					}
				}
			}
		}, 1000);

	return entries;
}

static std::shared_future<bool> makeReadySharedFuture(bool value) {
	std::promise<bool> promise;
	promise.set_value(value);
	return promise.get_future().share();
}

static bool listfile_check_cache_expiry(int64_t last_modified) {
	if (last_modified > 0) {
		int64_t ttl = 0;
		if (core::view->config.contains("listfileCacheRefresh")) {
			try {
				ttl = core::view->config["listfileCacheRefresh"].get<int64_t>();
			} catch (const nlohmann::json::type_error&) {
				ttl = 0;
			}
		}

		ttl *= 24LL * 60 * 60 * 1000;

		if (ttl == 0) {
			logging::write(std::format("Cached listfile is out-of-date (> {}).", ttl));
			return true;
		}

		auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		if ((now - last_modified) > ttl) {
			logging::write(std::format("Cached listfile is out-of-date (> {}).", ttl));
			return true;
		}

		logging::write("Listfile is cached locally.");
		return false;
	}

	logging::write("Listfile is not cached, downloading fresh.");
	return true;
}

static int64_t getFileLastModifiedMs(const fs::path& file_path) {
	try {
		auto ftime = fs::last_write_time(file_path);
		auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			sctp.time_since_epoch()).count();
	} catch (...) {
		return 0;
	}
}

static bool listfile_preload_legacy() {
	try {
		std::string url;
		if (core::view->config.contains("listfileURL"))
			url = core::view->config["listfileURL"].get<std::string>();

		if (url.empty())
			throw std::runtime_error("Missing/malformed listfileURL in configuration!");

		fs::create_directories(constants::CACHE::DIR_LISTFILE());

		auto cache_file = constants::CACHE::DIR_LISTFILE() / std::string(constants::CACHE::LISTFILE_DATA);

		preloadedIdLookup.clear();
		preloadedNameLookup.clear();

		BufferWrapper data;
		bool have_data = false;

		if (url.starts_with("http")) {
			BufferWrapper cached;
			bool have_cached = false;
			int64_t last_modified = 0;

			try {
				cached = BufferWrapper::readFile(cache_file);
				have_cached = true;
				last_modified = getFileLastModifiedMs(cache_file);
			} catch (...) {
				// No cached file
			}

			if (listfile_check_cache_expiry(last_modified)) {
				try {
					std::string fallback_url;
					if (core::view->config.contains("listfileFallbackURL"))
						fallback_url = core::view->config["listfileFallbackURL"].get<std::string>();

					auto pct_pos = fallback_url.find("%s");
					if (pct_pos != std::string::npos)
						fallback_url.erase(pct_pos, 2);

					data = generics::downloadFile({url, fallback_url});
					have_data = true;

					data.writeToFile(cache_file);
				} catch (const std::exception& e) {
					if (!have_cached) {
						logging::write(std::format("Failed to download listfile during preload, no cached version for fallback: {}", e.what()));
						return false;
					}

					logging::write(std::format("Failed to download listfile during preload, using cached version: {}", e.what()));
					data = std::move(cached);
					have_data = true;
				}
			} else {
				data = std::move(cached);
				have_data = true;
			}
		} else {
			logging::write(std::format("Preloading user-defined local listfile: {}", url));
			data = BufferWrapper::readFile(url);
			have_data = true;
		}

		if (!have_data)
			return false;

		auto lines = data.readLines();
		logging::write(std::format("Processing {} listfile lines in chunks...", lines.size()));

		generics::batchWork<std::string>(std::string_view("listfile parsing"), lines,
			[&](const std::string& line, size_t) {
				if (line.empty())
					return;

				auto semicolon_pos = line.find(';');
				if (semicolon_pos == std::string::npos) {
					logging::write("Invalid listfile line (token count): " + line);
					return;
				}

				if (line.find(';', semicolon_pos + 1) != std::string::npos) {
					logging::write("Invalid listfile line (token count): " + line);
					return;
				}

				std::string id_str = line.substr(0, semicolon_pos);
				uint32_t fileDataID;
				try {
					fileDataID = static_cast<uint32_t>(std::stoul(id_str));
				} catch (...) {
					logging::write("Invalid listfile line (non-numerical ID): " + line);
					return;
				}

				std::string fileName = line.substr(semicolon_pos + 1);
				std::transform(fileName.begin(), fileName.end(), fileName.begin(),
					[](unsigned char c) { return std::tolower(c); });

				preloadedIdLookup[fileDataID] = fileName;
				preloadedNameLookup[fileName] = fileDataID;
			}, 1000);

		if (preloadedIdLookup.empty()) {
			logging::write("No entries found in preloaded listfile");
			return false;
		}

		preload_textures_ids = getFileDataIDsByExtension({ExtFilter(".blp")}, "filtering textures");

		preload_sounds_ids = getFileDataIDsByExtension(
			{ExtFilter(".ogg"), ExtFilter(".mp3"), ExtFilter(".unk_sound")}, "filtering sounds");

		preload_text_ids = getFileDataIDsByExtension(
			{ExtFilter(".txt"), ExtFilter(".lua"), ExtFilter(".xml"), ExtFilter(".sbt"),
			 ExtFilter(".wtf"), ExtFilter(".htm"), ExtFilter(".toc"), ExtFilter(".xsd")},
			"filtering text files");

		preload_fonts_ids = getFileDataIDsByExtension({ExtFilter(".ttf")}, "filtering fonts");

		preload_models_ids = getFileDataIDsByExtension(
			{ExtFilter(".m2"), ExtFilter(".m3"), ExtFilter(".wmo", constants::LISTFILE_MODEL_FILTER())},
			"filtering models");

		is_preloaded = true;
		logging::write(std::format("Preloaded {} listfile entries and filtered by extensions", preloadedIdLookup.size()));
		return true;
	} catch (const std::exception& e) {
		logging::write(std::format("Error during listfile preload: {}", e.what()));
		is_preloaded = false;
		return false;
	}
}

static bool listfile_preload_impl() {
	is_preloaded = false;
	logging::write("Preloading master listfile...");
	return listfile_preload_legacy();
}

std::shared_future<bool> preloadAsync() {
	std::lock_guard<std::mutex> guard(preload_mutex);

	if (preload_future.has_value())
		return *preload_future;

	if (is_preloaded)
		return makeReadySharedFuture(true);

	preload_future = std::async(std::launch::async, []() {
		return listfile_preload_impl();
	}).share();

	return *preload_future;
}

bool preload() {
	return preloadAsync().get();
}

std::shared_future<bool> prepareListfileAsync() {
	{
		std::lock_guard<std::mutex> guard(preload_mutex);

		if (is_preloaded)
			return makeReadySharedFuture(true);

		if (preload_future.has_value()) {
			logging::write("Waiting for listfile preload to complete...");
			return *preload_future;
		}
	}

	logging::write("Starting listfile preload...");
	return preloadAsync();
}

bool prepareListfile() {
	return prepareListfileAsync().get();
}

static size_t loadIDTable(const std::unordered_set<uint32_t>& ids, const std::string& ext) {
	size_t loadCount = 0;

	std::unique_lock lock(legacy_data_mutex);
	for (uint32_t fileDataID : ids) {
		if (!legacy_id_lookup.contains(fileDataID)) {
			std::string fileName = "unknown/" + std::to_string(fileDataID) + ext;
			legacy_id_lookup[fileDataID] = fileName;
			legacy_name_lookup[fileName] = fileDataID;
			loadCount++;
		}
	}

	return loadCount;
}

size_t loadUnknownTextures() {
	db::caches::DBTextureFileData::ensureInitialized();
	const auto& ids = db::caches::DBTextureFileData::getFileDataIDs();
	size_t unkBlp = loadIDTable(ids, ".blp");
	logging::write(std::format("Added {} unknown BLP textures from TextureFileData to listfile", unkBlp));
	return unkBlp;
}

std::future<size_t> loadUnknownTexturesAsync() {
	return std::async(std::launch::async, []() {
		return loadUnknownTextures();
	});
}

size_t loadUnknownModels() {
	const auto& ids = db::caches::DBModelFileData::getFileDataIDs();
	size_t unkM2 = loadIDTable(ids, ".m2");
	logging::write(std::format("Added {} unknown M2 models from ModelFileData to listfile", unkM2));
	return unkM2;
}

std::future<size_t> loadUnknownModelsAsync() {
	return std::async(std::launch::async, []() {
		return loadUnknownModels();
	});
}

void loadUnknowns() {
	loadUnknownModels();
}

std::future<void> loadUnknownsAsync() {
	return std::async(std::launch::async, []() {
		loadUnknowns();
	});
}

bool existsByID(uint32_t id) {
	std::shared_lock lock(legacy_data_mutex);
	return legacy_id_lookup.contains(id);
}

std::optional<std::string> getByID(uint32_t id) {
	std::shared_lock lock(legacy_data_mutex);
	auto it = legacy_id_lookup.find(id);
	if (it != legacy_id_lookup.end())
		return it->second;

	return std::nullopt;
}

std::string getByIDOrUnknown(uint32_t id, const std::string& ext) {
	auto result = getByID(id);
	if (result.has_value())
		return *result;

	return formatUnknownFile(id, ext);
}

std::optional<uint32_t> getByFilename(const std::string& filename) {
	std::string lower = filename;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return std::tolower(c); });
	std::replace(lower.begin(), lower.end(), '\\', '/');

	std::optional<uint32_t> lookup;

	{
		std::shared_lock lock(legacy_data_mutex);
		auto it = legacy_name_lookup.find(lower);
		if (it != legacy_name_lookup.end())
			lookup = it->second;
	}

	if (!lookup.has_value() && (lower.ends_with(".mdl") || lower.ends_with("mdx")))
		return getByFilename(ExportHelper::replaceExtension(lower, ".m2"));

	return lookup;
}

std::vector<std::string> getFilenamesByExtension(const std::vector<ExtFilter>& exts) {
	std::vector<uint32_t> entries;

	{
		std::shared_lock lock(legacy_data_mutex);
		for (const auto& [fileDataID, fn] : legacy_id_lookup) {
			for (const auto& ext : exts) {
				if (ext.has_exclusion && ext.exclusion_regex) {
					if (fn.ends_with(ext.ext) && !std::regex_search(fn, *ext.exclusion_regex)) {
						entries.push_back(fileDataID);
						break;
					}
				} else {
					if (fn.ends_with(ext.ext)) {
						entries.push_back(fileDataID);
						break;
					}
				}
			}
		}
	}

	return formatEntries(entries);
}

std::vector<std::string> formatEntries(std::vector<uint32_t>& file_data_ids) {
	bool sort_by_id = false;
	if (core::view->config.contains("listfileSortByID"))
		sort_by_id = core::view->config["listfileSortByID"].get<bool>();

	if (sort_by_id)
		std::sort(file_data_ids.begin(), file_data_ids.end());

	const size_t n_entries = file_data_ids.size();
	std::vector<std::string> entries(n_entries);

	for (size_t i = 0; i < n_entries; i++) {
		const uint32_t fid = file_data_ids[i];
		entries[i] = getByIDOrUnknown(fid) + " [" + std::to_string(fid) + "]";
	}

	if (!sort_by_id)
		std::sort(entries.begin(), entries.end());

	return entries;
}

std::optional<int> applyPreload(const std::unordered_set<uint32_t>& rootEntries) {
	if (!is_preloaded) {
		logging::write("No preloaded listfile available, falling back to normal loading");
		return 0;
	}

	try {
		logging::write("Applying preloaded listfile data...");

		size_t valid_entries = 0;
		{
			std::unique_lock lock(legacy_data_mutex);
			for (const auto& [fileDataID, fileName] : preloadedIdLookup) {
				if (rootEntries.contains(fileDataID)) {
					legacy_id_lookup[fileDataID] = fileName;
					legacy_name_lookup[fileName] = fileDataID;
					valid_entries++;
				}
			}
		}

		core::view->listfileTextures.clear();
		auto tex = formatEntries(preload_textures_ids);
		for (auto& s : tex) core::view->listfileTextures.emplace_back(std::move(s));

		auto snd = formatEntries(preload_sounds_ids);
		core::view->listfileSounds.clear();
		for (auto& s : snd) core::view->listfileSounds.emplace_back(std::move(s));

		auto txt = formatEntries(preload_text_ids);
		core::view->listfileText.clear();
		for (auto& s : txt) core::view->listfileText.emplace_back(std::move(s));

		auto fnt = formatEntries(preload_fonts_ids);
		core::view->listfileFonts.clear();
		for (auto& s : fnt) core::view->listfileFonts.emplace_back(std::move(s));

		auto mdl = formatEntries(preload_models_ids);
		core::view->listfileModels.clear();
		for (auto& s : mdl) core::view->listfileModels.emplace_back(std::move(s));

		if (valid_entries == 0) {
			logging::write("No preloaded entries matched rootEntries");
			return 0;
		}

		loaded = true;
		logging::write(std::format("Applied {} preloaded listfile entries", valid_entries));
	} catch (const std::exception& e) {
		logging::write(std::format("Error applying preloaded listfile: {}", e.what()));
	}

	return std::nullopt;
}

static std::vector<FilteredEntry> getFilteredEntriesImpl(const std::string* search, const std::regex* re) {
	std::vector<FilteredEntry> results;

	auto matches = [&](const std::string& fileName) -> bool {
		if (re != nullptr)
			return std::regex_search(fileName, *re);
		return fileName.find(*search) != std::string::npos;
	};

	std::shared_lock lock(legacy_data_mutex);
	for (const auto& [fileDataID, fileName] : legacy_id_lookup) {
		if (matches(fileName))
			results.push_back({fileDataID, fileName});
	}

	return results;
}

std::vector<FilteredEntry> getFilteredEntries(const std::string& search) {
	return getFilteredEntriesImpl(&search, nullptr);
}

std::vector<FilteredEntry> getFilteredEntries(const std::regex& search) {
	return getFilteredEntriesImpl(nullptr, &search);
}

std::vector<std::string> renderListfile(const std::optional<std::vector<uint32_t>>& file_data_ids,
                                         bool include_main_index) {
	std::vector<std::string> result;
	const bool has_id_filter = file_data_ids.has_value();

	std::unordered_set<uint32_t> id_set;
	std::unordered_set<uint32_t> seen_ids;
	if (has_id_filter)
		id_set.insert(file_data_ids->begin(), file_data_ids->end());

	{
		std::shared_lock lock(legacy_data_mutex);
		if (!has_id_filter) {
			for (const auto& [file_data_id, fn] : legacy_id_lookup) {
				result.push_back(fn + " [" + std::to_string(file_data_id) + "]");
			}
		} else {
			for (const auto& [file_data_id, fn] : legacy_id_lookup) {
				if (id_set.contains(file_data_id)) {
					result.push_back(fn + " [" + std::to_string(file_data_id) + "]");
					seen_ids.insert(file_data_id);
				}
			}
		}
	}

	// append unnamed entries for IDs not found in any data source
	if (has_id_filter) {
		for (const uint32_t id : *file_data_ids) {
			if (!seen_ids.contains(id))
				result.push_back("unknown/" + std::to_string(id) + " [" + std::to_string(id) + "]");
		}
	}

	return result;
}

std::future<std::vector<std::string>> renderListfileAsync(const std::optional<std::vector<uint32_t>>& file_data_ids,
                                                          bool include_main_index) {
	return std::async(std::launch::async, [file_data_ids, include_main_index]() {
		return renderListfile(file_data_ids, include_main_index);
	});
}

std::string stripFileEntry(const std::string& entry) {
	auto pos = entry.rfind(" [");
	if (pos != std::string::npos)
		return entry.substr(0, pos);

	return entry;
}

ParsedEntry parseFileEntry(const std::string& entry) {
	ParsedEntry result;
	result.file_path = stripFileEntry(entry);

	static const std::regex fid_regex(R"(\[(\d+)\]$)");
	std::smatch match;
	if (std::regex_search(entry, match, fid_regex))
		result.file_data_id = static_cast<uint32_t>(std::stoul(match[1].str()));

	return result;
}

std::string formatUnknownFile(uint32_t fileDataID, const std::string& ext) {
	return "unknown/" + std::to_string(fileDataID) + ext;
}

bool isLoaded() {
	return loaded;
}

void ingestIdentifiedFiles(const std::vector<std::pair<uint32_t, std::string>>& entries) {
	std::unique_lock lock(legacy_data_mutex);
	for (const auto& [fileDataID, ext] : entries) {
		std::string fileName = "unknown/" + std::to_string(fileDataID) + ext;
		legacy_id_lookup[fileDataID] = fileName;
		legacy_name_lookup[fileName] = fileDataID;
	}
}

void addEntry(uint32_t fileDataID, const std::string& fileName,
              std::vector<std::string>* listfile_out) {
	std::string lower = fileName;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return std::tolower(c); });

	{
		std::unique_lock lock(legacy_data_mutex);
		legacy_id_lookup[fileDataID] = lower;
		legacy_name_lookup[lower] = fileDataID;
	}

	if (listfile_out)
		listfile_out->push_back(lower + " [" + std::to_string(fileDataID) + "]");
}

} // namespace listfile
} // namespace casc
