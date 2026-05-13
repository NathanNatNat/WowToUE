/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
 */
#include "dbd-manifest.h"

#include "../core.h"
#include "../log.h"
#include "../generics.h"
#include "../buffer.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <format>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

std::atomic<bool> is_preloaded{false};
std::optional<std::shared_future<void>> preload_promise;
std::unordered_map<std::string, uint32_t> table_to_id;
std::unordered_map<uint32_t, std::string> id_to_table;
std::mutex manifest_mutex;
std::once_flag preload_once_flag;

}

namespace casc {
namespace dbd_manifest {

/**
 * preload the dbd manifest from configured urls
 */
void preload() {
	std::call_once(preload_once_flag, []() {
		preload_promise = std::async(std::launch::async, []() {
			try {
				const std::string dbd_filename_url = core::view->config["dbdFilenameURL"].get<std::string>();
				const std::string dbd_filename_fallback_url = core::view->config["dbdFilenameFallbackURL"].get<std::string>();

				auto raw = generics::downloadFile(std::vector<std::string>{ dbd_filename_url, dbd_filename_fallback_url });
				const nlohmann::json manifest_data = raw.readJSON();

				{
					std::lock_guard<std::mutex> lock(manifest_mutex);
					for (const auto& entry : manifest_data) {
						if (!entry.contains("tableName") || !entry.contains("db2FileDataID"))
							continue;

						const auto& tn = entry["tableName"];
						const auto& fid = entry["db2FileDataID"];

						if (!tn.is_string() || tn.get<std::string>().empty())
							continue;
						if (!fid.is_number_integer() || fid.get<int64_t>() == 0)
							continue;

						const std::string table_name = tn.get<std::string>();
						const uint32_t file_data_id = fid.get<uint32_t>();
						table_to_id[table_name] = file_data_id;
						id_to_table[file_data_id] = table_name;
					}
				}

				logging::write(std::format("preloaded dbd manifest with {} entries", table_to_id.size()));
				is_preloaded.store(true);
			} catch (const std::exception& e) {
				logging::write(std::format("failed to preload dbd manifest: {}", e.what()));
				is_preloaded.store(true);
			}
		}).share();
	});
}

/**
 * prepare the manifest for use, awaiting preload if necessary
 * @returns {Promise<boolean>}
 */
bool prepareManifest() {
	if (is_preloaded.load())
		return true;

	if (!preload_promise.has_value())
		preload();

	preload_promise->wait();
	return true;
}

/**
 * get table name by filedataid
 * @param {number} id
 * @returns {string|undefined}
 */
std::optional<std::string> getByID(uint32_t id) {
	std::lock_guard<std::mutex> lock(manifest_mutex);
	auto it = id_to_table.find(id);
	if (it != id_to_table.end())
		return it->second;
	return std::nullopt;
}

/**
 * get filedataid by table name
 * @param {string} table_name
 * @returns {number|undefined}
 */
std::optional<uint32_t> getByTableName(const std::string& table_name) {
	std::lock_guard<std::mutex> lock(manifest_mutex);
	auto it = table_to_id.find(table_name);
	if (it != table_to_id.end())
		return it->second;
	return std::nullopt;
}

/**
 * get all table names
 * @returns {string[]}
 */
std::vector<std::string> getAllTableNames() {
	std::lock_guard<std::mutex> lock(manifest_mutex);
	std::vector<std::string> names;
	names.reserve(table_to_id.size());
	for (const auto& [key, value] : table_to_id)
		names.push_back(key);
	std::sort(names.begin(), names.end());
	return names;
}

}
}
