/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "version-config.h"

#include <cctype>

namespace casc {

std::vector<std::unordered_map<std::string, std::string>> parseVersionConfig(std::string_view data) {
	std::vector<std::unordered_map<std::string, std::string>> entries;

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

	if (lines.empty())
		return entries;

	// First line contains field definitions.
	// Example: Name!STRING:0|Path!STRING:0|Hosts!STRING:0|Servers!STRING:0|ConfigPath!STRING:0
	std::string_view header_line = lines[0];
	std::vector<std::string> fields;

	{
		std::size_t pos = 0;
		while (pos < header_line.size()) {
			std::size_t pipe = header_line.find('|', pos);
			if (pipe == std::string_view::npos)
				pipe = header_line.size();

			std::string_view header = header_line.substr(pos, pipe - pos);

			std::size_t bang = header.find('!');
			if (bang != std::string_view::npos)
				header = header.substr(0, bang);

			// Whitespace is replaced so that a field like 'Install Key' becomes 'InstallKey'.
			// This just improves coding readability when accessing the fields later on.
			std::string field(header);
			auto space_pos = field.find(' ');
			if (space_pos != std::string::npos)
				field.erase(space_pos, 1);
			fields.push_back(std::move(field));

			pos = pipe + 1;
		}
	}

	for (std::size_t li = 1; li < lines.size(); li++) {
		std::string_view entry = lines[li];

		// Skip empty lines/comments.
		std::string_view trimmed = entry;
		while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())))
			trimmed.remove_prefix(1);
		while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))
			trimmed.remove_suffix(1);

		if (trimmed.empty() || entry.front() == '#')
			continue;

		std::unordered_map<std::string, std::string> node;
		std::size_t pos = 0;
		std::size_t fi = 0;

		while (pos <= entry.size()) {
			std::size_t pipe = entry.find('|', pos);
			if (pipe == std::string_view::npos)
				pipe = entry.size();

			const std::string key = fi < fields.size() ? fields[fi] : "undefined";
			node[key] = std::string(entry.substr(pos, pipe - pos));
			fi++;
			pos = pipe + 1;
		}

		entries.push_back(std::move(node));
	}

	return entries;
}

} // namespace casc
