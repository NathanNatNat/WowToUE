/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

// Dedicated translation unit for stb_image implementation.
// STB_IMAGE_IMPLEMENTATION must be defined in exactly one .cpp file
// to emit the function definitions. All other files that need stb_image.h
// should include it without this macro.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// STB_IMAGE_RESIZE_IMPLEMENTATION is defined here (not in ADTExporter.cpp)
// to avoid ODR violations if any other TU also includes stb_image_resize2.h.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4668)
#endif
#include <stb_image_resize2.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
