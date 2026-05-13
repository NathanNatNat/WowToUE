/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace geoset_mapper {

/**
 * Get the label for a geoset based on the group.
 * @param index
 * @param id
 */
std::string getGeosetName(int index, int id);

/**
 * Geoset entry used by the map function.
 */
struct Geoset {
	int id = 0;
	std::string label;
};

/**
 * Map geoset names for subMeshes.
 * @param geosets
 */
void map(std::vector<Geoset>& geosets);

} // namespace geoset_mapper
