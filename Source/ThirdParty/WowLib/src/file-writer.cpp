/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "file-writer.h"
#include <memory>
#include <stdexcept>

FileWriter::FileWriter(const std::filesystem::path& file, std::string_view /*encoding*/)
	: write_mutex(std::make_unique<std::mutex>()) {
	stream.open(file, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		if (!std::filesystem::is_directory(file))
			throw std::runtime_error("failed to open file for writing: " + file.string());

		std::filesystem::remove_all(file);
		stream.open(file, std::ios::out | std::ios::binary | std::ios::trunc);

		if (!stream.is_open())
			throw std::runtime_error("failed to open file after removing directory: " + file.string());
	}
}

FileWriter::~FileWriter() {
	try {
		close();
	} catch (...) {
	}
}

void FileWriter::writeLine(std::string_view line) {
	std::lock_guard lock(*write_mutex);

	if (closed || !stream.is_open())
		throw std::runtime_error("write after end");

	stream << line << '\n';
	if (!stream)
		throw std::runtime_error("failed to write line");
}

void FileWriter::close() {
	if (!write_mutex)
		return;

	std::lock_guard lock(*write_mutex);

	closed = true;

	if (stream.is_open())
		stream.close();
}
