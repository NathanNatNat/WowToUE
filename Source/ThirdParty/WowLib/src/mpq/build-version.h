/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string>
#include <vector>

namespace mpq {

/// Default build version (Classic 1.12.1)
inline constexpr const char* DEFAULT_BUILD = "1.12.1.5875";

/**
 * Detect build version for an MPQ install directory.
 *
 * JS equivalent: function detect_build_version — module.exports = { detect_build_version, DEFAULT_BUILD }
 *
 * @param directory The MPQ install directory.
 * @param mpq_files List of MPQ file paths.
 * @return Build version string (e.g. "3.3.5.12340").
 */
std::string detect_build_version(const std::string& directory, const std::vector<std::string>& mpq_files);

} // namespace mpq
