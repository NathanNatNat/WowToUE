/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string>

/**
 * Application configuration loading, saving, and reset.
 *
 * JS equivalent: module.exports = { load, resetToDefault, resetAllToDefault }
 */
namespace config {

/**
 * Load configuration from disk.
 * Reads default config, then merges with user config overrides.
 */
void load();

/**
 * Reset a configuration key to default.
 * @param key Configuration key to reset.
 */
void resetToDefault(const std::string& key);

/**
 * Reset all configuration to default.
 */
void resetAllToDefault();

/**
 * Mark configuration for saving.
 * Called whenever config changes and needs to be persisted.
 */
void save();

/**
 * Check if config has changed since last save and trigger a save if so.
 * Called each frame from the main loop, equivalent to JS core.view.$watch('config', ..., { deep: true }).
 */
void checkForChanges();

} // namespace config
