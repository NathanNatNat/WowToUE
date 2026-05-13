/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <format>
#include <string>
#include <string_view>

namespace logging {

/// Initialize the logging stream. Must be called once at startup
/// after constants::init(), which provides constants::RUNTIME_LOG().
/// If not called, write() will lazily initialize the stream on first use,
/// matching JS behavior where the stream is created at module-load time.
void init();

/**
 * Write a message to the log.
 *
 * JS equivalent: write(...parameters) uses util.format(...parameters).
 * The base overload accepts a pre-formatted string. The variadic template
 * overload below accepts a format string + arguments, matching the JS API.
 */
void write(std::string_view message);

/**
 * Variadic overload matching JS write(...parameters) with util.format().
 * Allows callers to write: logging::write("pattern {} {}", arg1, arg2)
 * instead of: logging::write(std::format("pattern {} {}", arg1, arg2))
 */
template<typename Arg, typename... Args>
void write(std::format_string<Arg, Args...> fmt, Arg&& arg, Args&&... args) {
	write(std::format(fmt, std::forward<Arg>(arg), std::forward<Args>(args)...));
}

/**
 * Internally mark the current timestamp for measuring
 * performance times with logging::timeEnd().
 */
void timeLog();

/**
 * Logs the time (in milliseconds) between the last logging::timeLog()
 * call and this call, with the given label prefixed.
 *
 * JS equivalent: timeEnd(label, ...params) passes additional variadic
 * parameters through to write(). In C++, the base overload accepts a
 * pre-formatted label. The variadic template overload below accepts a
 * format string + arguments, matching the JS API.
 *
 * @param label Label to prefix the time output.
 */
void timeEnd(std::string_view label);

/**
 * Variadic overload matching JS timeEnd(label, ...params).
 * The format string and arguments are used to build the label; the
 * elapsed time is appended automatically, matching JS behavior where
 * write(label + ' (%dms)', ...params, elapsed) is called internally.
 */
template<typename Arg, typename... Args>
void timeEnd(std::format_string<Arg, Args...> fmt, Arg&& arg, Args&&... args) {
	timeEnd(std::string_view(std::format(fmt, std::forward<Arg>(arg), std::forward<Args>(args)...)));
}

/**
 * Flush remaining pooled log entries to the stream.
 *
 * JS equivalent: the Node.js stream 'drain' event + process.nextTick
 * scheduling ensures pooled entries are eventually written. In C++,
 * call flush() at shutdown to ensure no entries are left in the pool.
 */
void flush();

/**
 * Open the runtime log in the user's external editor.
 */
void openRuntimeLog();

void setCallback(void (*callback)(const char*));

} // namespace logging

/**
 * Attempts to return the contents of the runtime log.
 * This is defined as a global as it is requested during
 * an application crash where modules may not be loaded.
 */
std::string getErrorDump();
