/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace casc {

/**
 * Parse a CDN/Build config file.
 *
 * Config files start with a "# " header comment line, followed by
 * key = value lines. Keys like 'encoding-sizes' are normalized to
 * camelCase ('encodingSizes').
 *
 * @param data Raw config file content.
 * @return Map of normalized key → value strings.
 * @throws std::runtime_error on invalid format.
 */
std::unordered_map<std::string, std::string> parseCDNConfig(std::string_view data);

} // namespace casc
