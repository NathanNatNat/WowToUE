/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "cdn-config.h"

#include <cctype>
#include <regex>
#include <stdexcept>
#include <vector>

namespace {

const std::regex KEY_VAR_PATTERN(R"(([^\s]+)\s?=\s?(.*))");

std::string normalizeKey(const std::string& key) {
	std::vector<std::string> keyParts;
	std::size_t start = 0;
	for (std::size_t i = 0; i <= key.size(); i++) {
		if (i == key.size() || key[i] == '-') {
			keyParts.push_back(key.substr(start, i - start));
			start = i + 1;
		}
	}

	// Nothing to split, just use the normal key.
	if (keyParts.size() == 1)
		return key;

	for (std::size_t i = 1, n = keyParts.size(); i < n; i++) {
		std::string& part = keyParts[i];
		if (!part.empty())
			part[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(part[0])));
	}

	std::string result;
	for (const auto& part : keyParts)
		result += part;

	return result;
}

} // anonymous namespace

namespace casc {

std::unordered_map<std::string, std::string> parseCDNConfig(std::string_view data) {
	std::unordered_map<std::string, std::string> entries;

	std::vector<std::string_view> lines;
	std::size_t start = 0;
	for (std::size_t i = 0; i < data.size(); i++) {
		if (data[i] == '\n') {
			std::string_view line = data.substr(start, i - start);
			if (!line.empty() && line.back() == '\r')
				line.remove_suffix(1);
			lines.push_back(line);
			start = i + 1;
		}
	}
	if (start <= data.size()) {
		std::string_view line = data.substr(start);
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);
		lines.push_back(line);
	}

	// Valid config files start with "# " (e.g., "# Build Configuration", "# CDN Configuration")
	auto trim = [](std::string_view sv) -> std::string_view {
		while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
			sv.remove_prefix(1);
		while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
			sv.remove_suffix(1);
		return sv;
	};

	bool hasValidHeader = !lines.empty() && trim(lines[0]).substr(0, 2) == "# ";

	if (!hasValidHeader)
		throw std::runtime_error("Invalid CDN config: unexpected start of config");

	for (const auto& lineView : lines) {
		// Skip empty lines/comments.
		if (trim(lineView).empty() || lineView.front() == '#')
			continue;

		std::string line(lineView);
		std::smatch match;
		if (!std::regex_search(line, match, KEY_VAR_PATTERN))
			throw std::runtime_error("Invalid token encountered parsing CDN config");

		entries[normalizeKey(match[1].str())] = match[2].str();
	}

	return entries;
}

} // namespace casc