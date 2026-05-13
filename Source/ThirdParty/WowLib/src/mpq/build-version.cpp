/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "build-version.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <format>

#include "../log.h"

namespace mpq {

namespace fs = std::filesystem;

// known mpq files -> expansion build defaults
static const std::unordered_map<std::string, std::string> EXPANSION_BUILDS = {
	{ "wotlk", "3.3.5.12340" },
	{ "tbc", "2.4.3.8606" },
	{ "vanilla", DEFAULT_BUILD }
};

static constexpr uint32_t VS_FIXEDFILEINFO_SIGNATURE = 0xFEEF04BD;

/**
 * parse VS_FIXEDFILEINFO from buffer, extract file version
 * @param buf
 * @param offset
 * @returns version string or empty
 */
static std::string parse_vs_fixed_file_info(const std::vector<uint8_t>& buf, size_t offset) {
	if (offset + 52 > buf.size())
		return {};

	auto readUInt32LE = [&](size_t pos) -> uint32_t {
		return static_cast<uint32_t>(buf[pos]) |
		       (static_cast<uint32_t>(buf[pos + 1]) << 8) |
		       (static_cast<uint32_t>(buf[pos + 2]) << 16) |
		       (static_cast<uint32_t>(buf[pos + 3]) << 24);
	};

	const uint32_t signature = readUInt32LE(offset);
	if (signature != VS_FIXEDFILEINFO_SIGNATURE)
		return {};

	const uint32_t file_version_ms = readUInt32LE(offset + 8);
	const uint32_t file_version_ls = readUInt32LE(offset + 12);

	const uint32_t major = (file_version_ms >> 16) & 0xFFFF;
	const uint32_t minor = file_version_ms & 0xFFFF;
	const uint32_t build = (file_version_ls >> 16) & 0xFFFF;
	const uint32_t revision = file_version_ls & 0xFFFF;

	return std::format("{}.{}.{}.{}", major, minor, build, revision);
}

/**
 * search buffer for VS_FIXEDFILEINFO signature and parse version
 * @param buf
 * @returns version string or empty
 */
static std::string find_version_in_buffer(const std::vector<uint8_t>& buf) {
	// search for signature 0xFEEF04BD (little-endian: BD 04 EF FE)
	const std::array<uint8_t, 4> sig_bytes = {{ 0xBD, 0x04, 0xEF, 0xFE }};

	if (buf.size() < 52)
		return {};

	size_t pos = 0;
	while (pos < buf.size() - 52) {
		auto it = std::search(buf.begin() + pos, buf.end(),
		                      sig_bytes.begin(), sig_bytes.end());
		if (it == buf.end())
			break;

		const size_t idx = static_cast<size_t>(std::distance(buf.begin(), it));

		const std::string version = parse_vs_fixed_file_info(buf, idx);
		if (!version.empty())
			return version;

		pos = idx + 1;
	}

	return {};
}

/**
 * attempt to read version from WoW.exe PE file
 * @param exe_path
 * @returns version string or empty
 */
static std::string read_exe_version(const fs::path& exe_path) {
	try {
		std::ifstream file(exe_path, std::ios::binary | std::ios::ate);
		if (!file.is_open())
			return {};

		const auto file_size = file.tellg();
		if (file_size < 64)
			return {};

		file.seekg(0);
		std::vector<uint8_t> buf(static_cast<size_t>(file_size));
		file.read(reinterpret_cast<char*>(buf.data()), file_size);

		// basic PE validation
		// check DOS header magic (MZ)
		const uint16_t mz_magic = static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8);
		if (mz_magic != 0x5A4D)
			return {};

		const std::string version = find_version_in_buffer(buf);
		if (!version.empty())
			logging::write(std::format("Detected build version from {}: {}", exe_path.filename().string(), version));

		return version;
	} catch (...) {
		return {};
	}
}

/**
 * find WoW executable in install directory
 * @param directory
 * @returns path or empty
 */
static fs::path find_wow_exe(const fs::path& directory) {
	const std::array<const char*, 4> candidates = {{ "WoW.exe", "WowClassic.exe", "Wow.exe", "wow.exe" }};
	const std::array<fs::path, 2> search_dirs = {{ directory, directory.parent_path() }};

	for (const auto& dir : search_dirs) {
		for (const auto& exe_name : candidates) {
			const fs::path exe_path = dir / exe_name;
			if (fs::exists(exe_path))
				return exe_path;
		}
	}

	return {};
}

/**
 * infer expansion from mpq file names
 * @param mpq_names - mpq file names
 * @returns expansion identifier
 */
static std::string infer_expansion_from_mpqs(const std::vector<std::string>& mpq_names) {
	std::unordered_set<std::string> names_set;

	for (const auto& n : mpq_names) {
		std::string basename = fs::path(n).filename().string();
		std::transform(basename.begin(), basename.end(), basename.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		names_set.insert(basename);
	}

	// wotlk indicators
	if (names_set.contains("lichking.mpq") || names_set.contains("expansion2.mpq"))
		return "wotlk";

	// tbc indicators
	if (names_set.contains("expansion.mpq"))
		return "tbc";

	return "vanilla";
}

/**
 * detect build version for an MPQ install directory
 * @param directory - the MPQ install directory
 * @param mpq_files - list of mpq file paths
 * @returns build version string
 */
std::string detect_build_version(const std::string& directory, const std::vector<std::string>& mpq_files) {
	// try reading from WoW.exe first
	const fs::path exe_path = find_wow_exe(fs::path(directory));
	if (!exe_path.empty()) {
		const std::string exe_version = read_exe_version(exe_path);
		if (!exe_version.empty())
			return exe_version;
	}

	// fallback: infer from mpq files
	const std::string expansion = infer_expansion_from_mpqs(mpq_files);
	const auto it = EXPANSION_BUILDS.find(expansion);
	const std::string build = (it != EXPANSION_BUILDS.end()) ? it->second : std::string{DEFAULT_BUILD};
	logging::write(std::format("Inferred {} expansion from MPQ files, using build {}", expansion, build));
	return build;
}

}
