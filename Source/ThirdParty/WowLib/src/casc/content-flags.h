/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>

/**
 * CASC Content Flags
 */
namespace casc::content_flags {

constexpr uint32_t LoadOnWindows       = 0x8;
constexpr uint32_t LoadOnMacOS         = 0x10;
constexpr uint32_t LowViolence         = 0x80;
constexpr uint32_t DoNotLoad           = 0x100;
constexpr uint32_t UpdatePlugin        = 0x800;
constexpr uint32_t Encrypted           = 0x8000000;
constexpr uint32_t NoNameHash          = 0x10000000;
constexpr uint32_t UncommonResolution  = 0x20000000;
constexpr uint32_t Bundle              = 0x40000000;
constexpr uint32_t NoCompression       = 0x80000000;

} // namespace casc::content_flags
