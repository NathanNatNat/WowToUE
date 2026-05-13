/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

namespace db::caches::DBGuildTabard {

struct ColorRGB {
	uint32_t r = 0;
	uint32_t g = 0;
	uint32_t b = 0;
};

// item_id -> tier
extern const std::unordered_map<uint32_t, int> GUILD_TABARD_ITEM_IDS;

/**
 * Initialize guild tabard data from DB2 tables.
 */
void initialize();

/**
 * Ensure guild tabard data is initialized.
 */
void ensureInitialized();

/**
 * Check if an item ID is a guild tabard.
 */
bool isGuildTabard(uint32_t item_id);

/**
 * Get tabard tier for a given item ID.
 * @returns Tier (0, 1, or 2), or -1 if not a guild tabard.
 */
int getTabardTier(uint32_t item_id);

/**
 * Get background texture FileDataID.
 */
uint32_t getBackgroundFDID(int tier, int component, int color);

/**
 * Get border texture FileDataID.
 */
uint32_t getBorderFDID(int tier, int component, int border_id, int color);

/**
 * Get emblem texture FileDataID.
 */
uint32_t getEmblemFDID(int component, int emblem_id, int color);

/**
 * Get count of background colors.
 */
int getBackgroundColorCount();

/**
 * Get count of border styles for a given tier.
 */
int getBorderStyleCount(int tier);

/**
 * Get count of border colors.
 */
int getBorderColorCount();

/**
 * Get count of emblem designs.
 */
int getEmblemDesignCount();

/**
 * Get count of emblem colors.
 */
int getEmblemColorCount();

/**
 * Get background color map (color_id -> {r, g, b}).
 */
const std::map<uint32_t, ColorRGB>& getBackgroundColors();

/**
 * Get border color map (color_id -> {r, g, b}).
 */
const std::map<uint32_t, ColorRGB>& getBorderColors();

/**
 * Get emblem color map (color_id -> {r, g, b}).
 */
const std::map<uint32_t, ColorRGB>& getEmblemColors();

} // namespace db::caches::DBGuildTabard
