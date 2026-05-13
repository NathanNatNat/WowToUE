/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace casc {
namespace cdn_resolver {

	struct HostResult {
		std::string host;
		int64_t ping;
	};

	/**
	 * Start pre-resolution for a region if not already started.
	 * @param region Region tag (eu, us, etc)
	 * @param product Product to use for server config (defaults to "wow")
	 */
	void startPreResolution(const std::string& region, const std::string& product = "wow");

	/**
	 * Get the best host for a region with specific server config.
	 * @param region Region tag
	 * @param serverConfig Server configuration with Hosts and Path
	 * @return Best host URL string
	 */
	std::string getBestHost(const std::string& region, const std::unordered_map<std::string, std::string>& serverConfig);

	/**
	 * Get all available hosts for a region ranked by ping speed.
	 * Excludes hosts that have previously failed.
	 * @param region Region tag
	 * @param serverConfig Server configuration with Hosts and Path
	 * @return Array of host URL strings ranked by speed
	 */
	std::vector<std::string> getRankedHosts(const std::string& region, const std::unordered_map<std::string, std::string>& serverConfig);

	/**
	 * Mark a host as failed (e.g., due to censorship or invalid responses).
	 * @param host Host URL to mark as failed
	 */
	void markHostFailed(const std::string& host);

} // namespace cdn_resolver
} // namespace casc
