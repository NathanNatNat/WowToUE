/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#include "db2.h"
#include "../db/WDCReader.h"
#include "../buffer.h"

#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>

namespace casc {
namespace db2 {

struct CacheEntry {
	std::unique_ptr<db::WDCReader> reader;
	std::once_flag parseFlag;
	std::once_flag preloadFlag;

	void ensureParsed() {
		std::call_once(parseFlag, [this]() {
			reader->parse();
		});
	}

	void ensurePreloaded() {
		std::call_once(preloadFlag, [this]() {
			ensureParsed();
			reader->preload();
		});
	}
};

static std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache;
static std::mutex cache_mutex;

db::WDCReader& getTable(const std::string& table_name) {
	CacheEntry* entry_ptr = nullptr;
	{
		std::unique_lock lock(cache_mutex);
		auto it = cache.find(table_name);
		if (it != cache.end()) {
			entry_ptr = it->second.get();
		} else {
			const std::string file_path = "DBFilesClient/" + table_name + ".db2";
			auto entry = std::make_unique<CacheEntry>();
			entry->reader = std::make_unique<db::WDCReader>(file_path);
			entry_ptr = entry.get();
			cache.emplace(table_name, std::move(entry));
		}
	}
	entry_ptr->ensureParsed();
	return *entry_ptr->reader;
}

db::WDCReader& preloadTable(const std::string& table_name) {
	CacheEntry* entry_ptr = nullptr;
	{
		std::unique_lock lock(cache_mutex);
		auto it = cache.find(table_name);
		if (it != cache.end()) {
			entry_ptr = it->second.get();
		} else {
			const std::string file_path = "DBFilesClient/" + table_name + ".db2";
			auto entry = std::make_unique<CacheEntry>();
			entry->reader = std::make_unique<db::WDCReader>(file_path);
			entry_ptr = entry.get();
			cache.emplace(table_name, std::move(entry));
		}
	}
	entry_ptr->ensurePreloaded();
	return *entry_ptr->reader;
}

} // namespace db2
} // namespace casc
