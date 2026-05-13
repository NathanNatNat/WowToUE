/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "config.h"
#include "constants.h"
#include "generics.h"
#include "core.h"
#include "log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <stdexcept>

namespace config {

static bool isSaving = false;
static bool isQueued = false;
static nlohmann::json defaultConfig = nlohmann::json::object();
static nlohmann::json lastSavedSnapshot = nlohmann::json::object();

/**
 * Clone one config object into another.
 * Arrays are cloned rather than passed by reference.
 * @param src    Source JSON config.
 * @param target Target JSON config.
 */
static void copyConfig(const nlohmann::json& src, nlohmann::json& target) {
	for (auto it = src.begin(); it != src.end(); ++it) {
		if (it.value().is_array()) {
			// Clone array rather than passing reference.
			target[it.key()] = nlohmann::json(it.value());
		} else {
			// Pass everything else in wholemeal.
			target[it.key()] = it.value();
		}
	}
}

/**
 * Persist configuration data to disk.
 */
static void doSave() {
	nlohmann::json configSave = nlohmann::json::object();
	for (auto it = core::view->config.begin(); it != core::view->config.end(); ++it) {
		// Only persist configuration values that do not match defaults.
		if (!it.value().is_array() && defaultConfig.contains(it.key()) && defaultConfig[it.key()] == it.value())
			continue;

		configSave[it.key()] = it.value();
	}

	const std::string out = configSave.dump(1, '\t');
	std::ofstream file(constants::CONFIG::USER_PATH());
	if (!file.is_open()) {
		logging::write("Failed to write config file: " + constants::CONFIG::USER_PATH().string());
	} else {
		file << out;
		file.close();
		lastSavedSnapshot = core::view->config;
	}

	// If another save was attempted during this one, re-save.
	if (isQueued) {
		isQueued = false;
		doSave();
	} else {
		isSaving = false;
	}
}

/**
 * Load configuration from disk.
 */
void load() {
	auto defaultResult = generics::readJSON(constants::CONFIG::DEFAULT_PATH(), true);
	defaultConfig = defaultResult.value_or(nlohmann::json::object());

	nlohmann::json userConfig = nlohmann::json::object();
	try {
		auto userResult = generics::readJSON(constants::CONFIG::USER_PATH());
		userConfig = userResult.value_or(nlohmann::json::object());
	} catch (const std::runtime_error& e) {
		if (std::string_view(e.what()).starts_with("Permission denied")) {
			logging::write("Failed to load user config due to restricted permissions (EPERM)");
			core::setToast("info",
				"Restricted permissions detected. Restart wow.export.cpp using \"Run as Administrator\".",
				{}, -1, true);
		} else {
			throw;
		}
	}

	logging::write("Loaded config defaults: " + defaultConfig.dump());
	logging::write("Loaded user config: " + userConfig.dump());

	nlohmann::json mergedConfig = nlohmann::json::object();
	copyConfig(defaultConfig, mergedConfig);
	copyConfig(userConfig, mergedConfig);

	core::view->config = mergedConfig;
	lastSavedSnapshot = mergedConfig;
}

/**
 * Reset a configuration key to default.
 */
void resetToDefault(const std::string& key) {
	if (defaultConfig.contains(key)) {
		core::view->config[key] = defaultConfig[key];
		save();
	}
}

/**
 * Reset all configuration to default.
 */
void resetAllToDefault() {
	// Use JSON parse/stringify to ensure deep non-referenced clone.
	core::view->config = nlohmann::json::parse(defaultConfig.dump());
	save();
}

/**
 * Mark configuration for saving.
 */
void save() {
	if (!isSaving) {
		isSaving = true;
		core::postToMainThread(doSave);
	} else {
		// Queue another save.
		isQueued = true;
	}
}

void checkForChanges() {
	if (!core::view)
		return;

	if (core::view->config != lastSavedSnapshot)
		save();
}

} // namespace config