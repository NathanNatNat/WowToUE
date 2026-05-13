/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "BONELoader.h"
#include "../../buffer.h"

BONELoader::BONELoader(BufferWrapper& data)
	: isLoaded(false), data(data) {
}

// See: https://wowdev.wiki/BONE

/**
 * Load the bone file.
 */
void BONELoader::load() {
	// Prevent multiple loading of the same file.
	if (this->isLoaded == true)
		return;

	this->data.readUInt32LE(); // Version?

	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		switch (chunkID) {
			case CHUNK_BIDA: this->parse_chunk_bida(chunkSize); break; // Bone ID
			case CHUNK_BOMT: this->parse_chunk_bomt(chunkSize); break; // Bone offset matrices
		}

		// Ensure that we start at the next chunk exactly.
		this->data.seek(nextChunkPos);
	}

	this->isLoaded = true;
}

void BONELoader::parse_chunk_bida(uint32_t chunkSize) {
	this->boneIDs = this->data.readUInt16LE(chunkSize / 2);
}

void BONELoader::parse_chunk_bomt(uint32_t chunkSize) {
	const uint32_t amount = (chunkSize / 16) / 4;
	this->boneOffsetMatrices.resize(amount);
	for (uint32_t i = 0; i < amount; i++) {
		this->boneOffsetMatrices[i].resize(4);
		for (uint32_t j = 0; j < 4; j++)
			this->boneOffsetMatrices[i][j] = this->data.readFloatLE(4);
	}
}