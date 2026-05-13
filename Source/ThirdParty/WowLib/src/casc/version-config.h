/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace casc {

std::vector<std::unordered_map<std::string, std::string>> parseVersionConfig(std::string_view data);

} // namespace casc
