/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "DBGuildTabard.h"

#include "../../log.h"
#include "../../casc/db2.h"
#include "../WDCReader.h"

#include <format>
#include <unordered_set>

namespace db::caches::DBGuildTabard {

static uint32_t fieldToUint32(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<uint32_t>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<uint32_t>(*p);
	return 0;
}

static int fieldToInt(const db::FieldValue& val) {
	if (auto* p = std::get_if<int64_t>(&val))
		return static_cast<int>(*p);
	if (auto* p = std::get_if<uint64_t>(&val))
		return static_cast<int>(*p);
	if (auto* p = std::get_if<float>(&val))
		return static_cast<int>(*p);
	return 0;
}

// item_id -> tier
const std::unordered_map<uint32_t, int> GUILD_TABARD_ITEM_IDS = { {5976, 0}, {69209, 1}, {69210, 2} };

// tier-component-color -> FileDataID
static std::unordered_map<std::string, uint32_t> background_map;

// tier-component-borderID-color -> FileDataID
static std::unordered_map<std::string, uint32_t> border_map;

// component-emblemID-color -> FileDataID
static std::unordered_map<std::string, uint32_t> emblem_map;

// color_id -> { r, g, b }
static std::map<uint32_t, ColorRGB> background_colors_map;
static std::map<uint32_t, ColorRGB> border_colors_map;
static std::map<uint32_t, ColorRGB> emblem_colors_map;

static int background_color_count = 0;
static int border_style_counts[3] = {0, 0, 0}; // per tier
static int border_color_count = 0;
static int emblem_design_count = 0;
static int emblem_color_count = 0;

static bool is_initialized = false;

void initialize() {
	if (is_initialized)
		return;

	logging::write("Loading guild tabard data...");

	std::unordered_set<int> bg_colors;
	for (const auto& [_id, row] : casc::db2::preloadTable("GuildTabardBackground").getAllRows()) {
		(void)_id;
		int tier = fieldToInt(row.at("Tier"));
		int component = fieldToInt(row.at("Component"));
		int color = fieldToInt(row.at("Color"));
		uint32_t fdid = fieldToUint32(row.at("FileDataID"));
		background_map[std::format("{}-{}-{}", tier, component, color)] = fdid;
		bg_colors.insert(color);
	}
	background_color_count = static_cast<int>(bg_colors.size());

	std::unordered_set<int> border_styles[3];
	std::unordered_set<int> border_color_ids;
	for (const auto& [_id, row] : casc::db2::preloadTable("GuildTabardBorder").getAllRows()) {
		(void)_id;
		int tier = fieldToInt(row.at("Tier"));
		int component = fieldToInt(row.at("Component"));
		int borderID = fieldToInt(row.at("BorderID"));
		int color = fieldToInt(row.at("Color"));
		uint32_t fdid = fieldToUint32(row.at("FileDataID"));
		border_map[std::format("{}-{}-{}-{}", tier, component, borderID, color)] = fdid;
		if (tier >= 0 && tier < 3)
			border_styles[tier].insert(borderID);
		border_color_ids.insert(color);
	}
	for (int i = 0; i < 3; i++)
		border_style_counts[i] = static_cast<int>(border_styles[i].size());
	border_color_count = static_cast<int>(border_color_ids.size());

	std::unordered_set<int> emblem_designs;
	std::unordered_set<int> emblem_color_ids;
	for (const auto& [_id, row] : casc::db2::preloadTable("GuildTabardEmblem").getAllRows()) {
		(void)_id;
		int component = fieldToInt(row.at("Component"));
		int emblemID = fieldToInt(row.at("EmblemID"));
		int color = fieldToInt(row.at("Color"));
		uint32_t fdid = fieldToUint32(row.at("FileDataID"));
		emblem_map[std::format("{}-{}-{}", component, emblemID, color)] = fdid;
		emblem_designs.insert(emblemID);
		emblem_color_ids.insert(color);
	}
	emblem_design_count = static_cast<int>(emblem_designs.size());
	emblem_color_count = static_cast<int>(emblem_color_ids.size());

	// load actual RGB color values
	for (const auto& [id, row] : casc::db2::preloadTable("GuildColorBackground").getAllRows())
		background_colors_map[id] = { fieldToUint32(row.at("Red")), fieldToUint32(row.at("Green")), fieldToUint32(row.at("Blue")) };

	for (const auto& [id, row] : casc::db2::preloadTable("GuildColorBorder").getAllRows())
		border_colors_map[id] = { fieldToUint32(row.at("Red")), fieldToUint32(row.at("Green")), fieldToUint32(row.at("Blue")) };

	for (const auto& [id, row] : casc::db2::preloadTable("GuildColorEmblem").getAllRows())
		emblem_colors_map[id] = { fieldToUint32(row.at("Red")), fieldToUint32(row.at("Green")), fieldToUint32(row.at("Blue")) };

	logging::write(std::format("Loaded guild tabard data: {} backgrounds, {} borders, {} emblems", background_map.size(), border_map.size(), emblem_map.size()));
	is_initialized = true;
}

void ensureInitialized() {
	if (!is_initialized)
		initialize();
}

bool isGuildTabard(uint32_t item_id) {
	return GUILD_TABARD_ITEM_IDS.contains(item_id);
}

int getTabardTier(uint32_t item_id) {
	auto it = GUILD_TABARD_ITEM_IDS.find(item_id);
	if (it != GUILD_TABARD_ITEM_IDS.end())
		return it->second;
	return -1;
}

uint32_t getBackgroundFDID(int tier, int component, int color) {
	auto it = background_map.find(std::format("{}-{}-{}", tier, component, color));
	if (it != background_map.end())
		return it->second;
	return 0;
}

uint32_t getBorderFDID(int tier, int component, int border_id, int color) {
	auto it = border_map.find(std::format("{}-{}-{}-{}", tier, component, border_id, color));
	if (it != border_map.end())
		return it->second;
	return 0;
}

uint32_t getEmblemFDID(int component, int emblem_id, int color) {
	auto it = emblem_map.find(std::format("{}-{}-{}", component, emblem_id, color));
	if (it != emblem_map.end())
		return it->second;
	return 0;
}

int getBackgroundColorCount() {
	return background_color_count;
}

int getBorderStyleCount(int tier) {
	if (tier >= 0 && tier < 3)
		return border_style_counts[tier];
	return 0;
}

int getBorderColorCount() {
	return border_color_count;
}

int getEmblemDesignCount() {
	return emblem_design_count;
}

int getEmblemColorCount() {
	return emblem_color_count;
}

const std::map<uint32_t, ColorRGB>& getBackgroundColors() {
	return background_colors_map;
}

const std::map<uint32_t, ColorRGB>& getBorderColors() {
	return border_colors_map;
}

const std::map<uint32_t, ColorRGB>& getEmblemColors() {
	return emblem_colors_map;
}

} // namespace db::caches::DBGuildTabard
