/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "ANIMLoader.h"
#include "../../buffer.h"

ANIMLoader::ANIMLoader(BufferWrapper& data)
	: isLoaded(false), data(data) {
}

// See: https://wowdev.wiki/M2#.anim_files

/**
 * Load the animation file.
 */
void ANIMLoader::load(bool isChunked) {
	// Prevent multiple loading of the same file.
	if (this->isLoaded == true)
		return;

	if (!isChunked) {
		this->animData = this->data.readUInt8(this->data.remainingBytes());
		this->isLoaded = true;
		return;
	}

	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		switch (chunkID) {
			case CHUNK_AFM2: this->parse_chunk_afm2(chunkSize); break; // AFM2 old animation data or ??? if AFSA/AFSB are present
			case CHUNK_AFSA: this->parse_chunk_afsa(chunkSize); break; // Skeleton Attachment animation data
			case CHUNK_AFSB: this->parse_chunk_afsb(chunkSize); break; // Skeleton Bone animation data
		}

		// Ensure that we start at the next chunk exactly.
		this->data.seek(nextChunkPos);
	}

	this->isLoaded = true;
}

void ANIMLoader::parse_chunk_afm2(uint32_t chunkSize) {
	this->animData = this->data.readUInt8(chunkSize);
}

void ANIMLoader::parse_chunk_afsa(uint32_t chunkSize) {
	this->skeletonAttachmentData = this->data.readUInt8(chunkSize);
}

void ANIMLoader::parse_chunk_afsb(uint32_t chunkSize) {
	this->skeletonBoneData = this->data.readUInt8(chunkSize);
	this->hasSkeletonBoneData = true;
}