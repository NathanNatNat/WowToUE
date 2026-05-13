/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "LoaderGenerics.h"
#include "../../buffer.h"

/**
 * Process a null-terminated string block.
 * @param data
 * @param chunkSize
 */
std::map<uint32_t, std::string> ReadStringBlock(BufferWrapper& data, uint32_t chunkSize) {
	BufferWrapper chunk = data.readBuffer(chunkSize, false);
	const auto& raw = chunk.raw();
	std::map<uint32_t, std::string> entries;

	uint32_t readOfs = 0;
	for (uint32_t i = 0; i < chunkSize; i++) {
		if (raw[i] == 0x0) {
			// Skip padding bytes.
			if (readOfs == i) {
				readOfs += 1;
				continue;
			}

			std::string str(reinterpret_cast<const char*>(raw.data() + readOfs), i - readOfs);
			std::erase(str, '\0');
			entries[readOfs] = std::move(str);
			readOfs = i + 1;
		}
	}

	return entries;
}