/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <filesystem>

namespace mmap_util {

/**
 * Opaque handle to a memory-mapped file.
 * Wraps platform-specific memory mapping (mmap on Linux, MapViewOfFile on Windows).
 */
struct MmapObject {
	void* data = nullptr;      ///< Pointer to the mapped memory region.
	size_t size = 0;           ///< Size of the mapped region in bytes.
	bool isMapped = false;     ///< Whether the file is currently mapped.
	std::string lastError;     ///< Last error message from map() failure.

#ifdef _WIN32
	void* fileHandle = nullptr;    ///< Windows file handle (HANDLE).
	void* mappingHandle = nullptr; ///< Windows file mapping handle (HANDLE).
#else
	int fd = -1;               ///< File descriptor (POSIX).
#endif

	/**
	 * Map a file into memory.
	 * @param path Path to the file to map.
	 * @returns true on success, false on failure.
	 */
	bool map(const std::filesystem::path& path);

	/**
	 * Unmap the memory-mapped file, releasing resources.
	 */
	void unmap();

	~MmapObject();

	// Non-copyable
	MmapObject() = default;
	MmapObject(const MmapObject&) = delete;
	MmapObject& operator=(const MmapObject&) = delete;
	MmapObject(MmapObject&& other) noexcept;
	MmapObject& operator=(MmapObject&& other) noexcept;
};

/**
 * Create a memory-mapped file object and track it for cleanup.
 * @returns Pointer to the new MmapObject.
 */
MmapObject* create_virtual_file();

/**
 * Release all tracked memory-mapped files.
 * Swallows errors to ensure all files are attempted.
 */
void release_virtual_files();

} // namespace mmap_util
