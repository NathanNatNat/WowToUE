/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "Skin.h"
#include "../casc/listfile.h"
#include "../core.h"
#include "../casc/casc-source.h"
#include "../buffer.h"

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

static constexpr uint32_t MAGIC_SKIN = 0x4E494B53;

Skin::Skin(uint32_t fileDataID)
	: fileDataID(fileDataID),
	  fileName(casc::listfile::getByIDOrUnknown(fileDataID, ".skin")),
	  isLoaded(false) {
}

void Skin::load() {
	try {
		BufferWrapper data = core::view->casc->getVirtualFileByID(this->fileDataID);

		const uint32_t magic = data.readUInt32LE();
		if (magic != MAGIC_SKIN)
			throw std::runtime_error("Invalid magic: " + std::to_string(magic));

		const uint32_t indicesCount = data.readUInt32LE();
		const uint32_t indicesOfs = data.readUInt32LE();
		const uint32_t trianglesCount = data.readUInt32LE();
		const uint32_t trianglesOfs = data.readUInt32LE();
		const uint32_t propertiesCount = data.readUInt32LE();
		const uint32_t propertiesOfs = data.readUInt32LE();
		const uint32_t subMeshesCount = data.readUInt32LE();
		const uint32_t subMeshesOfs = data.readUInt32LE();
		const uint32_t textureUnitsCount = data.readUInt32LE();
		const uint32_t textureUnitsOfs = data.readUInt32LE();
		this->bones = data.readUInt32LE();

		// Read indices.
		data.seek(indicesOfs);
		this->indices.resize(indicesCount);
		for (uint32_t i = 0; i < indicesCount; i++)
			this->indices[i] = data.readUInt16LE();

		// Read triangles.
		data.seek(trianglesOfs);
		this->triangles.resize(trianglesCount);
		for (uint32_t i = 0; i < trianglesCount; i++)
			this->triangles[i] = data.readUInt16LE();

		// Read properties.
		data.seek(propertiesOfs);
		this->properties.resize(propertiesCount);
		for (uint32_t i = 0; i < propertiesCount; i++)
			this->properties[i] = data.readUInt8();

		// Read subMeshes.
		data.seek(subMeshesOfs);
		this->subMeshes.resize(subMeshesCount);
		for (uint32_t i = 0; i < subMeshesCount; i++) {
			SubMesh& sm = this->subMeshes[i];
			sm.submeshID = data.readUInt16LE();
			sm.level = data.readUInt16LE();
			sm.vertexStart = data.readUInt16LE();
			sm.vertexCount = data.readUInt16LE();
			sm.triangleStart = data.readUInt16LE();
			sm.triangleCount = data.readUInt16LE();
			sm.boneCount = data.readUInt16LE();
			sm.boneStart = data.readUInt16LE();
			sm.boneInfluences = data.readUInt16LE();
			sm.centerBoneIndex = data.readUInt16LE();
			sm.centerPosition[0] = data.readFloatLE();
			sm.centerPosition[1] = data.readFloatLE();
			sm.centerPosition[2] = data.readFloatLE();
			sm.sortCenterPosition[0] = data.readFloatLE();
			sm.sortCenterPosition[1] = data.readFloatLE();
			sm.sortCenterPosition[2] = data.readFloatLE();
			sm.sortRadius = data.readFloatLE();

			sm.triangleStart += sm.level << 16;
		}

		// Read texture units.
		data.seek(textureUnitsOfs);
		this->textureUnits.resize(textureUnitsCount);
		for (uint32_t i = 0; i < textureUnitsCount; i++) {
			TextureUnit& tu = this->textureUnits[i];
			tu.flags = data.readUInt8();
			tu.priority = data.readInt8();
			tu.shaderID = data.readUInt16LE();
			tu.skinSectionIndex = data.readUInt16LE();
			tu.flags2 = data.readUInt16LE();
			tu.colorIndex = data.readUInt16LE();
			tu.materialIndex = data.readUInt16LE();
			tu.materialLayer = data.readUInt16LE();
			tu.textureCount = data.readUInt16LE();
			tu.textureComboIndex = data.readUInt16LE();
			tu.textureCoordComboIndex = data.readUInt16LE();
			tu.textureWeightComboIndex = data.readUInt16LE();
			tu.textureTransformComboIndex = data.readUInt16LE();
		}

		this->isLoaded = true;
	} catch (const std::exception& e) {
		throw std::runtime_error(
			std::format("Unable to load skin fileDataID {}: {}", this->fileDataID, e.what()));
	}
}