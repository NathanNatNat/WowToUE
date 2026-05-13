/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <regex>
#include <filesystem>

namespace constants {

/// Must be called once at startup to initialize runtime paths and
/// perform legacy directory migrations.
void init();


const std::filesystem::path& INSTALL_PATH(); // Path to the application installation.
const std::filesystem::path& DATA_DIR(); // Path to the user data directory (JS: nw.App.dataPath).
const std::filesystem::path& LOG_DIR(); // Path to the application logs directory.
const std::filesystem::path& RUNTIME_LOG(); // Path to the runtime log (JS: DATA_PATH/runtime.log).
const std::filesystem::path& LAST_EXPORT(); // Location of the last export.
const std::filesystem::path& SRC_DIR(); // Path to bundled resources (JS: INSTALL_PATH/src/).

// Maximum recent local installations to remember.
inline constexpr int MAX_RECENT_LOCAL = 3;

// JS: path.join(INSTALL_PATH, 'src', 'shaders')
const std::filesystem::path& SHADER_PATH();

// Current version of wow.export.
// JS reads version dynamically from `nw.App.manifest.version`.
// C++ sources it from CMake (WOW_EXPORT_VERSION cache variable).
inline constexpr std::string_view VERSION = WOW_EXPORT_VERSION;

// Build flavour identifier (JS: nw.App.manifest.flavour).
// In the JS version this comes from the NW.js package manifest (e.g. "win-x64-debug").
// For the C++ port this is derived from compile-time platform + build config.
#ifdef _WIN32
  #define WOW_EXPORT_PLATFORM "win"
#else
  #define WOW_EXPORT_PLATFORM "linux"
#endif

#if defined(__x86_64__) || defined(_M_X64)
  #define WOW_EXPORT_ARCH "x64"
#elif defined(__aarch64__) || defined(_M_ARM64)
  #define WOW_EXPORT_ARCH "arm64"
#else
  #define WOW_EXPORT_ARCH "x86"
#endif

#ifndef WOW_EXPORT_BUILD_TYPE
  #define WOW_EXPORT_BUILD_TYPE "unknown"
#endif

inline constexpr std::string_view FLAVOUR = WOW_EXPORT_PLATFORM "-" WOW_EXPORT_ARCH "-" WOW_EXPORT_BUILD_TYPE;

// Build GUID (JS: nw.App.manifest.guid).
// Fixed at build time, identical across all runs of the same build — matching the JS
// version where guid is baked into the NW.js manifest. Must NOT change between launches;
// only change between releases so checkForUpdates() only triggers on genuine updates.
inline constexpr std::string_view BUILD_GUID = "3f6a2b1c-8e47-4d9a-b531-7c2e9f4a8d6b";

// Filter used to filter out WMO LOD files.
const std::regex& LISTFILE_MODEL_FILTER();

// User-agent used for HTTP/HTTPS requests.
const std::string& USER_AGENT();

namespace GAME {
	inline constexpr int MAP_SIZE = 64;
	inline constexpr int MAP_SIZE_SQ = 4096; // MAP_SIZE ^ 2
	inline constexpr double MAP_COORD_BASE = 51200.0 / 3.0;
	inline constexpr double TILE_SIZE = (51200.0 / 3.0) / 32.0;
	inline constexpr int MAP_OFFSET = 17066;
}

// JS: cache paths are under DATA_PATH/casc/.
namespace CACHE {
	const std::filesystem::path& DIR(); // Cache directory.
	const std::filesystem::path& SIZE(); // Cache size.
	const std::filesystem::path& INTEGRITY_FILE(); // Cache integrity file.
	inline constexpr int SIZE_UPDATE_DELAY = 5000; // Milliseconds to buffer cache size update writes.
	const std::filesystem::path& DIR_BUILDS(); // Build-specific cache directory.
	const std::filesystem::path& DIR_INDEXES(); // Cache for archive indexes.
	const std::filesystem::path& DIR_DATA(); // Cache for single data files.
	const std::filesystem::path& DIR_DBD(); // Cache for DBD files.
	const std::filesystem::path& DIR_LISTFILE(); // Master listfile cache directory.
	inline constexpr std::string_view BUILD_MANIFEST = "manifest.json"; // Build-specific manifest file.
	inline constexpr std::string_view BUILD_LISTFILE = "listfile"; // Build-specific listfile file.
	inline constexpr std::string_view BUILD_ENCODING = "encoding"; // Build-specific encoding file.
	inline constexpr std::string_view BUILD_ROOT = "root"; // Build-specific root file.
	inline constexpr std::string_view LISTFILE_DATA = "listfile.txt"; // Master listfile data file.
	const std::filesystem::path& TACT_KEYS(); // Tact key cache.
	const std::filesystem::path& REALMLIST(); // Realmlist cache.
	inline constexpr std::string_view SUBMIT_URL = "https://www.kruithne.net/wow.export/v2/cache/submit";
	inline constexpr std::string_view FINALIZE_URL = "https://www.kruithne.net/wow.export/v2/cache/finalize";
	const std::filesystem::path& STATE_FILE();
}

namespace CONFIG {
	const std::filesystem::path& DEFAULT_PATH(); // Path of default configuration file.
	const std::filesystem::path& USER_PATH(); // Path of user-defined configuration file.
}

// product: Internal product ID.
// title: Label as it appears on the Battle.net launcher.
// tag: Specific version tag.
struct Product {
	std::string_view product;
	std::string_view title;
	std::string_view tag;
};

inline constexpr std::array<Product, 11> PRODUCTS = {{
	{ "wow", "World of Warcraft", "Retail" },
	{ "wowt", "PTR: World of Warcraft", "PTR" },
	{ "wowxptr", "PTR 2: World of Warcraft", "PTR 2" },
	{ "wow_beta", "Beta: World of Warcraft", "Beta" },
	{ "wow_classic", "World of Warcraft Classic", "Classic" },
	{ "wow_classic_beta", "Beta: World of Warcraft Classic", "Classic Beta" },
	{ "wow_classic_ptr", "PTR: World of Warcraft Classic", "Classic PTR" },
	{ "wow_classic_era", "World of Warcraft Classic Era", "Classic Era" },
	{ "wow_classic_era_ptr", "PTR: World of Warcraft Classic Era", "Classic Era PTR" },
	{ "wow_classic_titan", "World of Warcraft Classic Titan Reforged", "Classic Titan" },
	{ "wow_anniversary", "World of Warcraft Classic Anniversary", "Classic Anniversary" }
}};

struct Region {
	std::string_view tag;
	std::string_view name;
};

namespace PATCH {
	inline constexpr std::array<Region, 5> REGIONS = {{
		{ "eu", "Europe" },
		{ "us", "Americas" },
		{ "kr", "Korea" },
		{ "tw", "Taiwan" },
		{ "cn", "China" }
	}};
	inline constexpr std::string_view DEFAULT_REGION = "us"; // Region which is selected by default.
	inline constexpr std::string_view HOST = "https://%s.version.battle.net/"; // Blizzard patch server host.
	inline constexpr std::string_view HOST_CHINA = "https://cn.version.battlenet.com.cn/"; // Blizzard China patch server host.
	inline constexpr std::string_view SERVER_CONFIG = "/cdns"; // CDN config file on patch server.
	inline constexpr std::string_view VERSION_CONFIG = "/versions"; // Versions config file on patch server.
}

namespace BUILD {
	inline constexpr std::string_view MANIFEST = ".build.info"; // File that contains version information in local installs.
	inline constexpr std::string_view DATA_DIR = "Data";
}

namespace TIME {
	inline constexpr int DAY = 86400000; // Milliseconds in a day.
}

namespace MAGIC {
	inline constexpr uint32_t M3DT = 0x5444334D; // M3 model magic.
	inline constexpr uint32_t MD21 = 0x3132444D; // M2 model magic.
	inline constexpr uint32_t MD20 = 0x3032444D; // M2 model magic (legacy)
}

struct FileIdentifier {
	std::array<std::string_view, 4> matches;
	int match_count;
	std::string_view ext;
};

extern const std::array<FileIdentifier, 17> FILE_IDENTIFIERS;

inline constexpr std::array<std::string_view, 22> NAV_BUTTON_ORDER = {{
	"tab_models",
	"tab_textures",
	"tab_characters",
	"tab_items",
	"tab_item_sets",
	"tab_decor",
	"tab_creatures",
	"tab_audio",
	"tab_maps",
	"tab_zones",
	"tab_text",
	"tab_fonts",
	"tab_data",
	"tab_models_legacy",
	"legacy_tab_textures",
	"legacy_tab_audio",
	"legacy_tab_fonts",
	"legacy_tab_data",
	"legacy_tab_files",
	"tab_raw",
	"tab_install",
	"settings"
}};

inline constexpr std::array<std::string_view, 2> CONTEXT_MENU_ORDER = {{
	"runtime-log",
	"restart"
}};

inline constexpr std::array<std::string_view, 14> FONT_PREVIEW_QUOTES = {{
	"You take no candle!",
	"Keep your feet on the ground.",
	"Me not that kind of orc!",
	"Time is money, friend.",
	"Something need doing?",
	"For the Horde!",
	"For the Alliance!",
	"Light be with you.",
	"Stay away from da voodoo.",
	"My magic will tear you apart!",
	"All I ever wanted to do was study!",
	"Put your faith in the light...",
	"Storm, earth, and fire! Heed my call!",
	"Avast ye swabs, repel the invaders!"
}};

struct Expansion {
	int id;
	std::string_view name;
	std::string_view shortName;
};

inline constexpr std::array<Expansion, 13> EXPANSIONS = {{
	{ 0, "Classic", "Classic" },
	{ 1, "The Burning Crusade", "TBC" },
	{ 2, "Wrath of the Lich King", "WotLK" },
	{ 3, "Cataclysm", "Cataclysm" },
	{ 4, "Mists of Pandaria", "MoP" },
	{ 5, "Warlords of Draenor", "WoD" },
	{ 6, "Legion", "Legion" },
	{ 7, "Battle for Azeroth", "BfA" },
	{ 8, "Shadowlands", "SL" },
	{ 9, "Dragonflight", "DF" },
	{ 10, "The War Within", "TWW" },
	{ 11, "Midnight", "Midnight" },
	{ 12, "The Last Titan", "TLT" }
}};

} // namespace constants
