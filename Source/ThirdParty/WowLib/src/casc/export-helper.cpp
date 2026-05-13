/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "export-helper.h"
#include "../core.h"
#include "../log.h"
#include "../generics.h"

#include <filesystem>
#include <format>
#include <algorithm>
#include <cctype>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <cstdlib>
#endif

namespace fs = std::filesystem;

namespace casc {

const std::vector<ToastAction> ExportHelper::TOAST_OPT_LOG = { {"View Log", []() { logging::openRuntimeLog(); }} };
//const nlohmann::json TOAST_OPT_DIR = { {"Open Export Directory", true} };

/**
 * Return an export path for the given file.
 * @param file
 */
std::string ExportHelper::getExportPath(const std::string& file) {
	std::string f = file;

	// Remove whitespace due to MTL incompatibility for textures.
	if (core::view->config.value("removePathSpaces", false))
		std::erase_if(f, [](char c) { return std::isspace(static_cast<unsigned char>(c)); });

	return (fs::path(core::view->config.value("exportDirectory", "")) / f).lexically_normal().string();
}

/**
 * Returns a relative path from the export directory to the given file.
 * @param file
 */
std::string ExportHelper::getRelativeExport(const std::string& file) {
	return fs::relative(fs::path(file), fs::path(core::view->config.value("exportDirectory", ""))).string();
}

/**
 * Takes the directory from fileA and combines it with the basename of fileB.
 * @param fileA
 * @param fileB
 */
std::string ExportHelper::replaceFile(const std::string& fileA, const std::string& fileB) {
	return (fs::path(fileA).parent_path() / fs::path(fileB).filename()).string();
}

/**
 * Replace an extension on a file path with another.
 * @param file
 * @param ext
 */
std::string ExportHelper::replaceExtension(const std::string& file, const std::string& ext) {
	return (fs::path(file).parent_path() / (fs::path(file).stem().string() + ext)).string();
}

/**
 * Replace the base name of a file path, keeping the directory and extension.
 * @param filePath
 * @param fileName
 */
std::string ExportHelper::replaceBaseName(const std::string& filePath, const std::string& fileName) {
	return (fs::path(filePath).parent_path() / (fileName + fs::path(filePath).extension().string())).string();
}

/**
 * Converts a win32 compatible path to a POSIX compatible path.
 * @param str
 */
std::string ExportHelper::win32ToPosix(std::string str) {
	// path module does not expose any decent conversion API, so simply
	// convert slashes like a cave-person and call it a day.
	std::replace(str.begin(), str.end(), '\\', '/');
	return str;
}

/**
 * Sanitize a string for use as a file name by removing invalid characters.
 * @param str
 */
std::string ExportHelper::sanitizeFilename(std::string str) {
	std::erase_if(str, [](char c) {
		return c == '\\' || c == '/' || c == ':' || c == '*' ||
		       c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
	});
	return str;
}

/**
 * Returns a filename with incremental suffix if the original already exists.
 * @param filePath The original file path
 * @returns The available filename with incremental suffix if needed
 */
std::string ExportHelper::getIncrementalFilename(const std::string& filePath) {
	if (!generics::fileExists(filePath))
		return filePath;

	const auto dir = fs::path(filePath).parent_path();
	const auto ext = fs::path(filePath).extension().string();
	const auto basename = fs::path(filePath).stem().string();

	int counter = 1;
	std::string newPath;

	do {
		newPath = (dir / std::format("{}_{}{}", basename, counter, ext)).string();
		counter++;
	} while (generics::fileExists(newPath));

	return newPath;
}

/**
 * Construct a new ExportHelper instance.
 * @param count
 * @param unit
 */
ExportHelper::ExportHelper(int count, const std::string& unit)
	: count(count), unit(unit) {
}

/**
 * How many items have failed to export.
 * @returns {number}
 */
int ExportHelper::failed() const {
	return count - succeeded;
}

/**
 * Get the unit name formatted depending on plurality.
 */
std::string ExportHelper::unitFormatted() const {
	return count > 1 ? unit + "s" : unit;
}

/**
 * Start the export.
 */
void ExportHelper::start() {
	succeeded = 0;
	isFinished = false;

	core::view->isBusy++;
	core::view->exportCancelled = false;

	logging::write(std::format("Starting export of {} {} items", count, unit));
	updateCurrentTask();

	core::events.once("toast-cancelled", [this]() {
		if (!isFinished) {
			core::setToast("progress", "Cancelling export, hold on...", {}, -1, false);
			core::view->exportCancelled = true;
		}
	});
}

/**
 * Returns true if the current export is cancelled. Also calls this->finish()
 * as we can assume the export will now stop.
 * @returns {boolean}
 */
bool ExportHelper::isCancelled() {
	if (core::view->exportCancelled) {
		finish();
		return true;
	}

	return false;
}

/**
 * Finish the export.
 * @param includeDirLink
 */
void ExportHelper::finish(bool includeDirLink) {
	// Prevent duplicate calls to finish() in the event of user cancellation.
	if (isFinished)
		return;
	
	logging::write(std::format("Finished export ({} succeeded, {} failed)", succeeded, failed()));

	if (succeeded == count) {
		// Everything succeeded.
		const std::string lastExportPath = ExportHelper::getExportPath(fs::path(lastItem).parent_path().string());
		std::vector<ToastAction> toastOpt = { {"View in Explorer", [lastExportPath]() { core::openInExplorer(lastExportPath); }} };

		if (count > 1)
			core::setToast("success", std::format("Successfully exported {} {}.", count, unitFormatted()), includeDirLink ? toastOpt : std::vector<ToastAction>{}, -1);
		else
			core::setToast("success", std::format("Successfully exported {}.", lastItem), includeDirLink ? toastOpt : std::vector<ToastAction>{}, -1);
	} else if (succeeded > 0) {
		// Partial success, not everything exported.
		const bool cancelled = core::view->exportCancelled;
		core::setToast("info", std::format("Export {} {} {} {} export.",
			cancelled ? "cancelled, " : "complete, but",
			failed(), unitFormatted(),
			cancelled ? "didn't" : "failed to"),
			cancelled ? std::vector<ToastAction>{} : TOAST_OPT_LOG);
	} else {
		// Everything failed.
		if (core::view->exportCancelled)
			core::setToast("info", "Export was cancelled by the user.");
		else
			core::setToast("error", std::format("Unable to export {}.", unitFormatted()), TOAST_OPT_LOG, -1);
	}

	isFinished = true;
	core::view->isBusy--;
}

/**
 * Set the current task name.
 * @param name
 */
void ExportHelper::setCurrentTaskName(const std::string& name) {
	currentTaskName = name;
	updateCurrentTask();
}

/**
 * Set the maximum value of the current task.
 * @param max
 */
void ExportHelper::setCurrentTaskMax(int max) {
	currentTaskMax = max;
	updateCurrentTask();
}

/**
 * Set the value of the current task.
 * @param value
 */
void ExportHelper::setCurrentTaskValue(int value) {
	currentTaskValue = value;
	updateCurrentTask();
}

/**
 * Clear the current progression task.
 */
void ExportHelper::clearCurrentTask() {
	currentTaskName = std::nullopt;
	currentTaskMax = -1;
	currentTaskValue = -1;
	updateCurrentTask();
}

/**
 * Update the current task progression.
 */
void ExportHelper::updateCurrentTask() {
	std::string exportProgress = std::format("Exporting {} / {} {}", succeeded, count, unitFormatted());

	if (currentTaskName.has_value()) {
		exportProgress += " (Current task: " + currentTaskName.value();
		if (currentTaskValue > -1 && currentTaskMax > -1)
			exportProgress += std::format(", {} / {}", currentTaskValue, currentTaskMax);

		exportProgress += ")";
	}

	core::setToast("progress", exportProgress, {}, -1, true);
}

/**
 * Mark exportation of an item.
 * @param item
 * @param state
 * @param error
 * @param stackTrace
 */
void ExportHelper::mark(const std::string& item, bool state, const std::string& error,
                        const std::optional<std::string>& stackTrace) {
	if (state) {
		logging::write(std::format("Successfully exported {}", item));
		lastItem = item;
		succeeded++;
	} else {
		logging::write(std::format("Failed to export {} ({})", item, error));

		if (stackTrace.has_value())
			std::cout << stackTrace.value() << '\n';
	}

	updateCurrentTask();
}

} // namespace casc
