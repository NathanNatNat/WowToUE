/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

// This file defines constants used throughout the application.
#include "constants.h"

#ifndef WOW_EXPORT_SOURCE_DIR
#define WOW_EXPORT_SOURCE_DIR "."
#endif

#include <filesystem>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <climits>
#endif

namespace constants {

namespace fs = std::filesystem;


static fs::path getExecutablePath() {
#ifdef _WIN32
	wchar_t buf[MAX_PATH];
	DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
	if (len == 0 || len >= MAX_PATH)
		return fs::current_path();
	return fs::path(std::wstring_view(buf, len));
#else
	char buf[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (len == -1)
		return fs::current_path();
	buf[len] = '\0';
	return fs::path(buf);
#endif
}


static fs::path getUserDataPath() {
#ifdef _WIN32
	const char* localAppData = std::getenv("LOCALAPPDATA");
	if (localAppData && localAppData[0])
		return fs::path(localAppData) / "wow.export.cpp" / "User Data" / "Default";
	return getExecutablePath().parent_path() / "data";
#else
	const char* xdgData = std::getenv("XDG_DATA_HOME");
	if (xdgData && xdgData[0])
		return fs::path(xdgData) / "wow.export.cpp" / "User Data" / "Default";
	const char* home = std::getenv("HOME");
	if (home && home[0])
		return fs::path(home) / ".local" / "share" / "wow.export.cpp" / "User Data" / "Default";
	return getExecutablePath().parent_path() / "data";
#endif
}

namespace {
	fs::path s_install_path;
	fs::path s_data_dir;
	fs::path s_src_dir;
	fs::path s_log_dir;
	fs::path s_runtime_log;
	fs::path s_last_export;
	fs::path s_shader_path;

	fs::path s_cache_dir;
	fs::path s_cache_size;
	fs::path s_cache_integrity_file;
	fs::path s_cache_dir_builds;
	fs::path s_cache_dir_indexes;
	fs::path s_cache_dir_data;
	fs::path s_cache_dir_dbd;
	fs::path s_cache_dir_listfile;
	fs::path s_cache_tact_keys;
	fs::path s_cache_realmlist;
	fs::path s_cache_state_file;

	fs::path s_config_default_path;
	fs::path s_config_user_path;

	std::string s_user_agent;
}


void init() {
	s_install_path = getExecutablePath().parent_path();

	s_data_dir = getUserDataPath();

	auto installSrc = s_install_path / "src";
	if (fs::exists(installSrc / "fonts"))
		s_src_dir = installSrc;
	else
		s_src_dir = fs::path(WOW_EXPORT_SOURCE_DIR) / "src";

	s_log_dir = s_data_dir;

	fs::create_directories(s_data_dir);

	s_runtime_log = s_data_dir / "runtime.log";
	s_last_export = s_data_dir / "last_export";

	s_shader_path = s_src_dir / "shaders";

	s_cache_dir = s_data_dir / "casc";
	s_cache_size = s_cache_dir / "cachesize";
	s_cache_integrity_file = s_cache_dir / "cacheintegrity";
	s_cache_dir_builds = s_cache_dir / "builds";
	s_cache_dir_indexes = s_cache_dir / "indices";
	s_cache_dir_data = s_cache_dir / "data";
	s_cache_dir_dbd = s_cache_dir / "dbd";
	s_cache_dir_listfile = s_cache_dir / "listfile";
	s_cache_tact_keys = s_data_dir / "tact.json";
	s_cache_realmlist = s_data_dir / "realmlist.json";
	s_cache_state_file = s_data_dir / "cache_state.json";

	s_config_default_path = s_src_dir / "default_config.jsonc";
	s_config_user_path = s_data_dir / "config.json";

	s_user_agent = std::string("wow.export (") + std::string(VERSION) + ")";
}


const fs::path& INSTALL_PATH() { return s_install_path; }
const fs::path& DATA_DIR() { return s_data_dir; }
const fs::path& SRC_DIR() { return s_src_dir; }
const fs::path& LOG_DIR() { return s_log_dir; }
const fs::path& RUNTIME_LOG() { return s_runtime_log; }
const fs::path& LAST_EXPORT() { return s_last_export; }
const fs::path& SHADER_PATH() { return s_shader_path; }

namespace CACHE {
	const fs::path& DIR() { return s_cache_dir; }
	const fs::path& SIZE() { return s_cache_size; }
	const fs::path& INTEGRITY_FILE() { return s_cache_integrity_file; }
	const fs::path& DIR_BUILDS() { return s_cache_dir_builds; }
	const fs::path& DIR_INDEXES() { return s_cache_dir_indexes; }
	const fs::path& DIR_DATA() { return s_cache_dir_data; }
	const fs::path& DIR_DBD() { return s_cache_dir_dbd; }
	const fs::path& DIR_LISTFILE() { return s_cache_dir_listfile; }
	const fs::path& TACT_KEYS() { return s_cache_tact_keys; }
	const fs::path& REALMLIST() { return s_cache_realmlist; }
	const fs::path& STATE_FILE() { return s_cache_state_file; }
}

namespace CONFIG {
	const fs::path& DEFAULT_PATH() { return s_config_default_path; }
	const fs::path& USER_PATH() { return s_config_user_path; }
}

const std::string& USER_AGENT() { return s_user_agent; }

const std::regex& LISTFILE_MODEL_FILTER() {
	static const std::regex re(R"((_\d\d\d_)|(_\d\d\d.wmo$)|(lod\d.wmo$))");
	return re;
}


using namespace std::string_view_literals;

const std::array<FileIdentifier, 17> FILE_IDENTIFIERS = {{
	{ {{"OggS"sv}}, 1, ".ogg" },
	{ {{"ID3"sv, "\xFF\xFB"sv, "\xFF\xF3"sv, "\xFF\xF2"sv}}, 4, ".mp3" },
	{ {{"AFM2"sv}}, 1, ".anim" },
	{ {{"AFSA"sv}}, 1, ".anim" },
	{ {{"AFSB"sv}}, 1, ".anim" },
	{ {{"BLP2"sv}}, 1, ".blp" },
	{ {{"MD20"sv}}, 1, ".m2" },
	{ {{"MD21"sv}}, 1, ".m2" },
	{ {{"M3DT"sv}}, 1, ".m3" },
	{ {{"SKIN"sv}}, 1, ".skin" },
	{ {{"\x01\x00\x00\x00""BIDA"sv}}, 1, ".bone" },
	{ {{"SYHP\x02\x00\x00\x00"sv}}, 1, ".phys" },
	{ {{"HSXG"sv}}, 1, ".bls" },
	{ {{"RVXT"sv}}, 1, ".tex" },
	{ {{"RIFF"sv}}, 1, ".avi" },
	{ {{"WDC3"sv}}, 1, ".db2" },
	{ {{"WDC4"sv}}, 1, ".db2" }
}};

} // namespace constants