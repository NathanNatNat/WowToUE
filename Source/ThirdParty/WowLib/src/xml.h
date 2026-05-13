/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string_view>
#include <nlohmann/json.hpp>

nlohmann::json parse_xml(std::string_view xml);
