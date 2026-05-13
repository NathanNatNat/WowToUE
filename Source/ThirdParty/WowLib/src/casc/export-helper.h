/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "../core.h"

namespace casc {

/**
 * ExportHelper is a unified way to provide feedback to the user and to
 * the logger about a generic export progress.
 */
class ExportHelper {
public:
	/// Toast option constant for viewing the log.
	static const std::vector<ToastAction> TOAST_OPT_LOG;

	// --- Static utility methods ---

	/**
	 * Return an export path for the given file.
	 * @param file
	 */
	static std::string getExportPath(const std::string& file);

	/**
	 * Returns a relative path from the export directory to the given file.
	 * @param file
	 * @returns relative path string
	 */
	static std::string getRelativeExport(const std::string& file);

	/**
	 * Takes the directory from fileA and combines it with the basename of fileB.
	 * @param fileA
	 * @param fileB
	 * @returns combined path string
	 */
	static std::string replaceFile(const std::string& fileA, const std::string& fileB);

	/**
	 * Replace an extension on a file path with another.
	 * @param file
	 * @param ext
	 * @returns path with replaced extension
	 */
	static std::string replaceExtension(const std::string& file, const std::string& ext = "");

	/**
	 * Replace the base name of a file path, keeping the directory and extension.
	 * @param filePath
	 * @param fileName
	 * @returns path with replaced base name
	 */
	static std::string replaceBaseName(const std::string& filePath, const std::string& fileName);

	/**
	 * Converts a win32 compatible path to a POSIX compatible path.
	 * @param str
	 * @returns POSIX path string
	 */
	static std::string win32ToPosix(std::string str);

	/**
	 * Sanitize a string for use as a file name by removing invalid characters.
	 * @param str
	 * @returns sanitized string
	 */
	static std::string sanitizeFilename(std::string str);

	/**
	 * Returns a filename with incremental suffix if the original already exists.
	 * @param filePath The original file path
	 * @returns The available filename with incremental suffix if needed
	 */
	static std::string getIncrementalFilename(const std::string& filePath);

	// --- Instance methods ---

	/**
	 * Construct a new ExportHelper instance.
	 * @param count
	 * @param unit
	 */
	explicit ExportHelper(int count, const std::string& unit = "item");

	/**
	 * How many items have failed to export.
	 * @returns failure count
	 */
	int failed() const;

	/**
	 * Get the unit name formatted depending on plurality.
	 * @returns unit name, pluralized if count > 1
	 */
	std::string unitFormatted() const;

	/**
	 * Start the export.
	 */
	void start();

	/**
	 * Returns true if the current export is cancelled. Also calls this->finish()
	 * as we can assume the export will now stop.
	 * @returns true if cancelled
	 */
	bool isCancelled();

	/**
	 * Finish the export.
	 * @param includeDirLink
	 */
	void finish(bool includeDirLink = true);

	/**
	 * Set the current task name.
	 * @param name
	 */
	void setCurrentTaskName(const std::string& name);

	/**
	 * Set the maximum value of the current task.
	 * @param max
	 */
	void setCurrentTaskMax(int max);

	/**
	 * Set the value of the current task.
	 * @param value
	 */
	void setCurrentTaskValue(int value);

	/**
	 * Clear the current progression task.
	 */
	void clearCurrentTask();

	/**
	 * Update the current task progression.
	 */
	void updateCurrentTask();

	/**
	 * Mark exportation of an item.
	 * @param item
	 * @param state
	 * @param error
	 * @param stackTrace
	 */
	void mark(const std::string& item, bool state, const std::string& error = "",
	          const std::optional<std::string>& stackTrace = std::nullopt);

private:
	int count;
	std::string unit;
	bool isFinished = false;
	int succeeded = 0;
	std::string lastItem;

	std::optional<std::string> currentTaskName;
	int currentTaskMax = -1;
	int currentTaskValue = -1;
};

} // namespace casc
