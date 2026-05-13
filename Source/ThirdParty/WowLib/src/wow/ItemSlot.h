/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <string_view>

namespace wow {

/**
 * Get the label for an item slot based on the ID.
 * @param id 
 */
std::string_view getSlotName(int id);

} // namespace wow
