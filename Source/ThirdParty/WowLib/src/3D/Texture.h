/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include "../buffer.h"

class Texture {
public:
	/**
	 * Construct a new Texture instance.
	 * @param flags
	 * @param fileDataID
	 */
	Texture(uint32_t flags, uint32_t fileDataID = 0);

	/**
	 * Set the texture file using a file name.
	 * @param fileName
	 */
	void setFileName(const std::string& fileName);

	/**
	 * Obtain the texture file for this texture, instance cached.
	 * Returns NULL if fileDataID is not set.
	 */
	std::optional<BufferWrapper> getTextureFile();

	uint32_t flags;
	uint32_t fileDataID;
	std::string fileName;

private:
	std::optional<BufferWrapper> data;
};
