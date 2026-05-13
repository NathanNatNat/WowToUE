/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "mmap.h"
#include "log.h"

#include <format>
#include <set>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

namespace mmap_util {


MmapObject::MmapObject(MmapObject&& other) noexcept
	: data(other.data),
	  size(other.size),
	  isMapped(other.isMapped),
	  lastError(std::move(other.lastError))
#ifdef _WIN32
	  , fileHandle(other.fileHandle)
	  , mappingHandle(other.mappingHandle)
#else
	  , fd(other.fd)
#endif
{
	other.data = nullptr;
	other.size = 0;
	other.isMapped = false;
#ifdef _WIN32
	other.fileHandle = nullptr;
	other.mappingHandle = nullptr;
#else
	other.fd = -1;
#endif
}

MmapObject& MmapObject::operator=(MmapObject&& other) noexcept {
	if (this != &other) {
		if (isMapped)
			unmap();

		data = other.data;
		size = other.size;
		isMapped = other.isMapped;
#ifdef _WIN32
		fileHandle = other.fileHandle;
		mappingHandle = other.mappingHandle;
		other.fileHandle = nullptr;
		other.mappingHandle = nullptr;
#else
		fd = other.fd;
		other.fd = -1;
#endif
		other.data = nullptr;
		other.size = 0;
		other.isMapped = false;
	}
	return *this;
}

bool MmapObject::map(const std::filesystem::path& path) {
	if (isMapped)
		unmap();

#ifdef _WIN32
	fileHandle = CreateFileW(
		path.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (fileHandle == INVALID_HANDLE_VALUE) {
		lastError = "CreateFile failed: " + std::to_string(GetLastError());
		fileHandle = nullptr;
		return false;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(fileHandle, &fileSize)) {
		lastError = "GetFileSizeEx failed: " + std::to_string(GetLastError());
		CloseHandle(fileHandle);
		fileHandle = nullptr;
		return false;
	}

	size = static_cast<size_t>(fileSize.QuadPart);

	if (size == 0) {
		data = nullptr;
		mappingHandle = nullptr;
		isMapped = true;
		lastError.clear();
		return true;
	}

	mappingHandle = CreateFileMappingW(
		fileHandle,
		nullptr,
		PAGE_READONLY,
		0,
		0,
		nullptr
	);

	if (!mappingHandle) {
		lastError = "CreateFileMapping failed: " + std::to_string(GetLastError());
		CloseHandle(fileHandle);
		fileHandle = nullptr;
		return false;
	}

	data = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
	if (!data) {
		lastError = "MapViewOfFile failed: " + std::to_string(GetLastError());
		CloseHandle(mappingHandle);
		CloseHandle(fileHandle);
		mappingHandle = nullptr;
		fileHandle = nullptr;
		return false;
	}
#else
	fd = open(path.c_str(), O_RDONLY);
	if (fd == -1) {
		lastError = std::string("open failed: ") + strerror(errno);
		return false;
	}

	struct stat st;
	if (fstat(fd, &st) == -1) {
		lastError = std::string("fstat failed: ") + strerror(errno);
		close(fd);
		fd = -1;
		return false;
	}

	size = static_cast<size_t>(st.st_size);

	if (size == 0) {
		data = nullptr;
		isMapped = true;
		lastError.clear();
		return true;
	}

	data = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		lastError = std::string("mmap failed: ") + strerror(errno);
		data = nullptr;
		close(fd);
		fd = -1;
		return false;
	}
#endif

	isMapped = true;
	return true;
}

void MmapObject::unmap() {
	if (!isMapped)
		return;

#ifdef _WIN32
	if (data)
		UnmapViewOfFile(data);
	if (mappingHandle)
		CloseHandle(mappingHandle);
	if (fileHandle)
		CloseHandle(fileHandle);

	mappingHandle = nullptr;
	fileHandle = nullptr;
#else
	if (data)
		munmap(data, size);
	if (fd != -1)
		close(fd);

	fd = -1;
#endif

	data = nullptr;
	size = 0;
	isMapped = false;
}

MmapObject::~MmapObject() {
	if (isMapped)
		unmap();
}


namespace {

std::mutex s_mutex;
std::set<MmapObject*> virtual_files;

}

/**
 * create memory-mapped file object and track it for cleanup.
 * @returns {object} mmap object
 */
MmapObject* create_virtual_file() {
	auto* mmap_obj = new MmapObject();

	std::lock_guard<std::mutex> lock(s_mutex);
	virtual_files.insert(mmap_obj);

	return mmap_obj;
}

/**
 * release all tracked memory-mapped files.
 * swallows errors to ensure all files are attempted.
 */
void release_virtual_files() {
	try {
		std::lock_guard<std::mutex> lock(s_mutex);

		for (auto* mmap_obj : virtual_files) {
			try {
				if (mmap_obj->isMapped)
					mmap_obj->unmap();
			} catch (...) {
				// swallow individual unmap errors
			}
			delete mmap_obj;
		}

		size_t count = virtual_files.size();
		virtual_files.clear();
		logging::write(std::format("released {} memory-mapped files", count));
	} catch (const std::exception& e) {
		logging::write(std::format("error during virtual file cleanup: {}", e.what()));
	} catch (...) {
		logging::write("error during virtual file cleanup");
	}
}

}
