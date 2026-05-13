#include "install-manifest.h"
#include "../buffer.h"

#include <cmath>
#include <stdexcept>

namespace {
constexpr uint16_t INSTALL_SIG = 0x4E49; // IN
}

namespace casc {

InstallManifest::InstallManifest(BufferWrapper& data) {
	parse(data);
}

void InstallManifest::parse(BufferWrapper& data) {
	if (data.readUInt16LE() != INSTALL_SIG)
		throw std::runtime_error("Invalid file signature for install manifest");

	version = data.readUInt8();
	hashSize = data.readUInt8();
	numTags = data.readUInt16BE();
	numFiles = data.readUInt32BE();

	tags.resize(numTags);
	files.resize(numFiles);

	maskSize = static_cast<uint32_t>(std::ceil(static_cast<double>(numFiles) / 8.0));

	for (uint16_t i = 0; i < numTags; i++) {
		tags[i].name = data.readNullTerminatedString();
		tags[i].type = data.readUInt16BE();
		tags[i].mask = data.readUInt8(maskSize);
	}

	for (uint32_t i = 0; i < numFiles; i++) {
		files[i].name = data.readNullTerminatedString();
		files[i].hash = data.readHexString(hashSize);
		files[i].size = data.readUInt32BE();
	}

	// Pre-compute tags.
	for (const auto& tag : tags) {
		const auto& mask = tag.mask;
		const auto n = mask.size();
		for (std::size_t i = 0; i < n; i++) {
			for (int j = 0; j < 8; j++) {
				if (((mask[i] >> (7 - j)) & 0x1) == 1) {
					std::size_t fileIdx = (i % n * 8) + static_cast<std::size_t>(j);
					if (fileIdx < files.size())
						files[fileIdx].tags.push_back(tag.name);
				}
			}
		}
	}
}

}