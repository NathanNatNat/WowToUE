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
#include <utility>

namespace casc::locale_flags {

struct LocaleEntry {
	std::string_view id;
	uint32_t flag;
	std::string_view name;
};

constexpr uint32_t enUS  = 0x2;
constexpr uint32_t koKR  = 0x4;
constexpr uint32_t frFR  = 0x10;
constexpr uint32_t deDE  = 0x20;
constexpr uint32_t zhCN  = 0x40;
constexpr uint32_t esES  = 0x80;
constexpr uint32_t zhTW  = 0x100;
constexpr uint32_t enGB  = 0x200;
// enCN = 0x400
// enTW = 0x800
constexpr uint32_t esMX  = 0x1000;
constexpr uint32_t ruRU  = 0x2000;
constexpr uint32_t ptBR  = 0x4000;
constexpr uint32_t itIT  = 0x8000;
constexpr uint32_t ptPT  = 0x10000;

constexpr std::array<LocaleEntry, 13> entries = {{
	{ "enUS", enUS, "American English [enUS]" },
	{ "koKR", koKR, "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4 [koKR]" },
	{ "frFR", frFR, "Fran\xc3\xa7" "ais [frFR]" },
	{ "deDE", deDE, "Deutsch [deDE]" },
	{ "zhCN", zhCN, "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87 [zhCN]" },
	{ "esES", esES, "Espa\xc3\xb1ol (Espa\xc3\xb1" "a) [esES]" },
	{ "zhTW", zhTW, "\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87 [zhTW]" },
	{ "enGB", enGB, "British English [enGB]" },
	// { "enCN", enCN, "Unknown [enCN]" },
	// { "enTW", enTW, "Unknown [enTW]" },
	{ "esMX", esMX, "Espa\xc3\xb1ol (Am\xc3\xa9rica Latina) [esMX]" },
	{ "ruRU", ruRU, "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9 [ruRU]" },
	{ "ptBR", ptBR, "Portugu\xc3\xaas (Brasil) [ptBR]" },
	{ "itIT", itIT, "Italiano [itIT]" },
	{ "ptPT", ptPT, "Portugu\xc3\xaas (Europeu) [ptPT]" },
}};

constexpr uint32_t getFlag(std::string_view id) {
	for (const auto& entry : entries)
		if (entry.id == id)
			return entry.flag;
	return 0;
}

constexpr std::string_view getName(std::string_view id) {
	for (const auto& entry : entries)
		if (entry.id == id)
			return entry.name;
	return {};
}

} // namespace casc::locale_flags
