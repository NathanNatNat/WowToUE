/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "xml.h"

#include <cctype>

namespace {

struct Parser {
	std::string_view xml;
	std::size_t pos = 0;

	void skip_whitespace() {
		while (pos < xml.size() && std::isspace(static_cast<unsigned char>(xml[pos])))
			pos++;
	}

	std::string parse_tag_name() {
		std::size_t start = pos;
		while (pos < xml.size()) {
			char ch = xml[pos];
			if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
				(ch >= '0' && ch <= '9') || ch == '_' || ch == ':' ||
				ch == '.' || ch == '-')
				pos++;
			else
				break;
		}
		return std::string(xml.substr(start, pos - start));
	}

	nlohmann::json parse_attributes() {
		nlohmann::json attrs = nlohmann::json::object();

		while (pos < xml.size()) {
			skip_whitespace();
			if (pos >= xml.size())
				break;

			if (xml[pos] == '>' || xml[pos] == '/' || xml[pos] == '?')
				break;

			std::size_t name_start = pos;
			while (pos < xml.size()) {
				char ch = xml[pos];
				if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
					(ch >= '0' && ch <= '9') || ch == '_' || ch == ':' ||
					ch == '.' || ch == '-')
					pos++;
				else
					break;
			}

			std::string name(xml.substr(name_start, pos - name_start));

			skip_whitespace();
			if (pos >= xml.size())
				break;

			if (xml[pos] == '=') {
				pos++;
				skip_whitespace();
				if (pos >= xml.size())
					break;

				char quote = xml[pos];
				pos++;

				std::size_t value_start = pos;
				while (pos < xml.size() && xml[pos] != quote)
					pos++;

				std::string value(xml.substr(value_start, pos - value_start));
				if (pos < xml.size())
					pos++;

				attrs["@_" + name] = value;
			}
		}

		return attrs;
	}

	struct Node {
		std::string tag;
		nlohmann::json attrs;
		std::vector<Node> children;
		bool self_closing = false;
		bool valid = false;
	};

	Node parse_node() {
		skip_whitespace();

		if (pos >= xml.size())
			return {};

		if (xml[pos] != '<')
			return {};

		pos++;
		if (pos >= xml.size())
			return {};

		// closing tag
		if (xml[pos] == '/') {
			while (pos < xml.size() && xml[pos] != '>')
				pos++;

			if (pos < xml.size())
				pos++;
			return {};
		}

		// processing instruction or xml declaration
		bool is_declaration = (pos < xml.size() && xml[pos] == '?');
		if (is_declaration)
			pos++;

		std::string tag_name = (is_declaration ? "?" : "") + parse_tag_name();
		nlohmann::json attrs = parse_attributes();

		// self-closing or declaration
		if (pos < xml.size() && (xml[pos] == '/' || xml[pos] == '?')) {
			pos++;
			if (pos < xml.size() && xml[pos] == '>')
				pos++;

			return { tag_name, attrs, {}, true, true };
		}

		if (pos < xml.size() && xml[pos] == '>')
			pos++;

		std::vector<Node> children;

		while (pos < xml.size()) {
			skip_whitespace();

			if (pos >= xml.size())
				break;

			// check for closing tag
			if (xml[pos] == '<' && pos + 1 < xml.size() && xml[pos + 1] == '/')
				break;

			Node child = parse_node();
			if (child.valid)
				children.push_back(std::move(child));
		}

		// skip closing tag
		if (pos < xml.size() && xml[pos] == '<' && pos + 1 < xml.size() && xml[pos + 1] == '/') {
			while (pos < xml.size() && xml[pos] != '>')
				pos++;

			if (pos < xml.size())
				pos++;
		}

		return { tag_name, attrs, std::move(children), false, true };
	}

	nlohmann::json build_object(const Node& node) {
		nlohmann::json obj = node.attrs;

		if (node.children.empty())
			return obj;

		std::unordered_map<std::string, std::vector<const Node*>> groups;

		for (const auto& child : node.children) {
			groups[child.tag].push_back(&child);
		}

		// build child objects
		for (const auto& [tag, nodes] : groups) {
			if (nodes.size() == 1) {
				obj[tag] = build_object(*nodes[0]);
			} else {
				nlohmann::json arr = nlohmann::json::array();
				for (const auto* n : nodes)
					arr.push_back(build_object(*n));
				obj[tag] = std::move(arr);
			}
		}

		return obj;
	}

	nlohmann::json parse() {
		nlohmann::json root = nlohmann::json::object();

		while (pos < xml.size()) {
			Node node = parse_node();

			if (node.valid)
				root[node.tag] = build_object(node);
		}

		return root;
	}
};

}

nlohmann::json parse_xml(std::string_view xml) {
	Parser parser{ xml, 0 };
	return parser.parse();
}
