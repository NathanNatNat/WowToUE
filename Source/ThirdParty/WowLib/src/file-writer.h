/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <fstream>
#include <string>
#include <string_view>
#include <filesystem>
#include <mutex>
#include <memory>

class FileWriter {
public:
	~FileWriter();
	FileWriter(const FileWriter&) = delete;
	FileWriter& operator=(const FileWriter&) = delete;
	FileWriter(FileWriter&&) noexcept = default;
	FileWriter& operator=(FileWriter&&) noexcept = default;

	FileWriter(const std::filesystem::path& file, std::string_view encoding = "utf8");

	bool isOpen() const { return stream.is_open(); }

	void writeLine(std::string_view line);

	void close();

private:
	std::ofstream stream;
	bool closed = false;
	std::unique_ptr<std::mutex> write_mutex;
};
