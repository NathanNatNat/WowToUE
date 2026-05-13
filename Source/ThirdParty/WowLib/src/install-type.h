/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>

namespace install_type {

enum InstallType : uint32_t {
	MPQ  = 1 << 0,
	CASC = 1 << 1
};

} // namespace install_type
