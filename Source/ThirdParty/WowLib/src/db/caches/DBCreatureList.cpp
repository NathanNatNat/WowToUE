/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBCreatureList.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <mutex>
#include <condition_variable>

namespace db::caches::DBCreatureList {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static std::string fieldToString(const db::FieldValue& val) {
	if (auto* p = std::get_if<std::string>(&val))
		return *p;
	return "";
}

static std::vector<uint32_t> fieldToUint32Vec(const db::FieldValue& val) {
	std::vector<uint32_t> result;
	if (auto* p = std::get_if<std::vector<int64_t>>(&val)) {
		for (auto v : *p)
			result.push_back(static_cast<uint32_t>(v));
	} else if (auto* p = std::get_if<std::vector<uint64_t>>(&val)) {
		for (auto v : *p)
			result.push_back(static_cast<uint32_t>(v));
	}
	return result;
}

static std::vector<CreatureEntry> creatures;
static std::unordered_map<uint32_t, size_t> creature_index;
static bool is_initialized = false;
static bool is_initializing = false;
static std::mutex init_mutex;
static std::condition_variable init_cv;

void initialize_creature_list() {
	{
		std::unique_lock lock(init_mutex);
		if (is_initialized)
			return;
		if (is_initializing) {
			init_cv.wait(lock, [] { return is_initialized || !is_initializing; });
			return;
		}
		is_initializing = true;
	}

	try {
		logging::write("Loading creature list...");

		auto allRows = casc::db2::preloadTable("Creature").getAllRows();
		for (const auto& [id, row] : allRows) {
			std::string name = fieldToString(row.at("Name_lang"));
			if (name.empty())
				continue;

			uint32_t display_id = 0;
			auto displayIDs = fieldToUint32Vec(row.at("DisplayID"));
			for (auto did : displayIDs) {
				if (did > 0) {
					display_id = did;
					break;
				}
			}

			std::vector<uint32_t> always_items;
			auto it = row.find("AlwaysItem");
			if (it != row.end()) {
				auto allItems = fieldToUint32Vec(it->second);
				for (auto item : allItems) {
					if (item > 0)
						always_items.push_back(item);
				}
			}

			creature_index[id] = creatures.size();
			creatures.emplace_back(CreatureEntry{id, std::move(name), display_id, std::move(always_items)});
		}

		logging::write(std::format("Loaded {} creatures", creatures.size()));
		{
			std::scoped_lock lock(init_mutex);
			is_initialized = true;
			is_initializing = false;
		}
		init_cv.notify_all();
	} catch (...) {
		{
			std::scoped_lock lock(init_mutex);
			is_initializing = false;
		}
		init_cv.notify_all();
		throw;
	}
}

std::future<void> initialize_creature_list_async() {
	return std::async(std::launch::async, [] { initialize_creature_list(); });
}

const std::vector<CreatureEntry>& get_all_creatures() {
	return creatures;
}

const CreatureEntry* get_creature_by_id(uint32_t id) {
	auto it = creature_index.find(id);
	if (it != creature_index.end() && it->second < creatures.size())
		return &creatures[it->second];
	return nullptr;
}

} // namespace db::caches::DBCreatureList
