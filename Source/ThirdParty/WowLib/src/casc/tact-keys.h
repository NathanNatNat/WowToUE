/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <future>

namespace casc {
namespace tact_keys {

std::optional<std::string> getKey(std::string_view keyName);

bool addKey(std::string_view keyName, std::string_view key);

void load();

void loadBackground();

void waitForLoad();

std::future<void> loadAsync();

} // namespace tact_keys
} // namespace casc
