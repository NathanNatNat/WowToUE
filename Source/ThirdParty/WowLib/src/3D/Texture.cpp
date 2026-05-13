/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "Texture.h"
#include "../casc/listfile.h"
#include "../core.h"
#include "../casc/casc-source.h"

Texture::Texture(uint32_t flags, uint32_t fileDataID)
	: flags(flags), fileDataID(fileDataID) {
}

/**
 * Set the texture file using a file name.
 * @param fileName
 */
void Texture::setFileName(const std::string& fileName) {
	auto id = casc::listfile::getByFilename(fileName);
	this->fileDataID = id.value_or(0);
}

/**
 * Obtain the texture file for this texture, instance cached.
 * Returns empty optional if fileDataID is not set.
 */
std::optional<BufferWrapper> Texture::getTextureFile() {
	if (this->fileDataID > 0) {
		if (!this->data.has_value()) {
			this->data = core::view->casc->getVirtualFileByID(this->fileDataID);
		}

		return this->data;
	}

	return std::nullopt;
}