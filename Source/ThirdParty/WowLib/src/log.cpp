/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT

	Modified for WowLib/UE integration: routes all logging to UE_LOG.
 */
#include "log.h"

#include <chrono>
#include <ctime>
#include <cstdio>
#include <mutex>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Log callback — WowCASCInterface sets this to route to UE_LOG
static void (*g_wowlib_log_callback)(const char* msg) = nullptr;

static void default_log(const char* msg)
{
	std::fputs(msg, stdout);
	std::fputs("\n", stdout);
}

#define WOWLIB_LOG(msg) do { if (g_wowlib_log_callback) g_wowlib_log_callback(msg); else default_log(msg); } while(0)

namespace {

std::chrono::steady_clock::time_point markTimer{};
std::mutex logMutex;

std::string getTimestamp() {
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	std::tm tm{};
#ifdef _WIN32
	localtime_s(&tm, &time);
#else
	localtime_r(&time, &tm);
#endif
	char buf[9];
	std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	return std::string(buf);
}

} // anonymous namespace

namespace logging {

void init() {
}

void write(std::string_view message) {
	std::lock_guard<std::mutex> lock(logMutex);
	std::string line = "[" + getTimestamp() + "] " + std::string(message);
	WOWLIB_LOG(line.c_str());
}

void timeLog() {
	markTimer = std::chrono::steady_clock::now();
}

void timeEnd(std::string_view label) {
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - markTimer).count();

	std::string line = "[" + getTimestamp() + "] " + std::string(label) +
		" (" + std::to_string(elapsed) + "ms)";
	WOWLIB_LOG(line.c_str());
}

void setCallback(void (*callback)(const char*)) {
	g_wowlib_log_callback = callback;
}

void flush() {
}

void openRuntimeLog() {
}

} // namespace logging

std::string getErrorDump() {
	return "WowLib running inside UE — no runtime log file.";
}
