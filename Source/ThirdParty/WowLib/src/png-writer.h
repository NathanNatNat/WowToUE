/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <filesystem>
#include <vector>
#include <future>

#include "buffer.h"

/**
 * PNG encoder class.
 * Constructs an RGBA PNG image and provides serialization to BufferWrapper or file.
 *
 * JS equivalent: class PNGWriter — module.exports = PNGWriter
 */
class PNGWriter {
public:
	/**
	 * Construct a new PNGWriter instance.
	 * @param width  Image width in pixels.
	 * @param height Image height in pixels.
	 */
	PNGWriter(uint32_t width, uint32_t height);

	/**
	 * Get the internal pixel data for this PNG.
	 */
	std::vector<uint8_t>& getPixelData();

	/**
	 * Encode the image data as a PNG and return it in a BufferWrapper.
	 * @returns BufferWrapper containing the complete PNG file.
	 */
	BufferWrapper getBuffer();

	/**
	 * Write this PNG to a file.
	 * @param file Path to write the PNG file to.
	 * @returns Shared future that resolves when file writing completes.
	 */
	std::shared_future<void> write(const std::filesystem::path& file);

	uint32_t width;
	uint32_t height;
	uint32_t bytesPerPixel;
	uint8_t bitDepth;
	uint8_t colorType;

private:
	std::vector<uint8_t> data;
};
