/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <mutex>
#include <future>

#include <nlohmann/json.hpp>

class BufferWrapper;

namespace casc {

/**
 * BuildCache — manages a per-build cache directory with integrity checking.
 *
 * JS equivalent: class BuildCache (build-cache.cpp) — module.exports = BuildCache
 */
class BuildCache {
public:
	/**
	 * Construct a new BuildCache instance.
	 * @param key Build key (MD5 hash string).
	 */
	explicit BuildCache(const std::string& key);

	/**
	 * Initialize the build cache instance.
	 * Creates the cache directory and loads the manifest.
	 */
	void init();
	std::future<void> initAsync();

	/**
	 * Attempt to get a file from this build cache.
	 * Returns std::nullopt if the file is not cached or integrity check fails.
	 * @param file File path relative to build cache.
	 * @param dir  Optional override directory.
	 */
	std::optional<BufferWrapper> getFile(const std::string& file, const std::string& dir = "");
	std::future<std::optional<BufferWrapper>> getFileAsync(const std::string& file, const std::string& dir = "");

	/**
	 * Get a direct path to a cached file.
	 * @param file File path relative to build cache.
	 * @param dir  Optional override directory.
	 */
	std::filesystem::path getFilePath(const std::string& file, const std::string& dir = "") const;

	/**
	 * Store a file in this build cache.
	 * @param file File path relative to build cache.
	 * @param data Data to store in the file.
	 * @param dir  Optional override directory.
	 */
	void storeFile(const std::string& file, BufferWrapper& data, const std::string& dir = "");
	std::future<void> storeFileAsync(const std::string& file, BufferWrapper data, const std::string& dir = "");

	/**
	 * Save the cache integrity to disk.
	 */
	void saveCacheIntegrity();
	std::future<void> saveCacheIntegrityAsync();

	/**
	 * Save the manifest for this build cache.
	 */
	void saveManifest();
	std::future<void> saveManifestAsync();

	/**
	 * Get the build key.
	 * JS equivalent: this.cache.key (accessed as a public property).
	 */
	const std::string& getKey() const { return key; }

private:
	std::string key;
	nlohmann::json meta;
	std::filesystem::path cacheDir;
	std::filesystem::path manifestPath;
};

/**
 * Initialize the build cache integrity system.
 * Loads the integrity file from disk. Must be called at startup.
 */
void initBuildCacheSystem();

/**
 * Register event handlers for cache clearing and stale cache cleanup.
 */
void registerBuildCacheEvents();

} // namespace casc
