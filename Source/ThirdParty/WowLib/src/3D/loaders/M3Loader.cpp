#include "M3Loader.h"
#include "../../buffer.h"
#include "../../log.h"

#include <format>
#include <stdexcept>

static constexpr uint32_t CHUNK_M3DT = 0x54444D33; // 'M3DT'
static constexpr uint32_t CHUNK_MES3 = 0x3353454D; // 'MES3'
static constexpr uint32_t CHUNK_M3VR = 0x52564333; // 'M3VR'
static constexpr uint32_t CHUNK_VPOS = 0x534F5056; // 'VPOS'
static constexpr uint32_t CHUNK_VNML = 0x4C4D4E56; // 'VNML'
static constexpr uint32_t CHUNK_VUV0 = 0x30565556; // 'VUV0'
static constexpr uint32_t CHUNK_VUV1 = 0x31565556; // 'VUV1'
static constexpr uint32_t CHUNK_VUV2 = 0x32565556; // 'VUV2'
static constexpr uint32_t CHUNK_VUV3 = 0x33565556; // 'VUV3'
static constexpr uint32_t CHUNK_VUV4 = 0x34565556; // 'VUV4'
static constexpr uint32_t CHUNK_VUV5 = 0x35565556; // 'VUV5'
static constexpr uint32_t CHUNK_VTAN = 0x4E415456; // 'VTAN'
static constexpr uint32_t CHUNK_VSTR = 0x52545356; // 'VSTR'
static constexpr uint32_t CHUNK_VINX = 0x584E4956; // 'VINX'
static constexpr uint32_t CHUNK_VGEO = 0x4F454756; // 'VGEO'
static constexpr uint32_t CHUNK_LODS = 0x53444F4C; // 'LODS'
static constexpr uint32_t CHUNK_RBAT = 0x54414252; // 'RBAT'
static constexpr uint32_t CHUNK_VWTS = 0x53545756; // 'VWTS'
static constexpr uint32_t CHUNK_VIBP = 0x50424956; // 'VIBP'
static constexpr uint32_t CHUNK_VCL0 = 0x304C4356; // 'VCL0'
static constexpr uint32_t CHUNK_VCL1 = 0x314C4356; // 'VCL1'
static constexpr uint32_t CHUNK_M3CL = 0x4C43334D; // 'M3CL'
static constexpr uint32_t CHUNK_CPOS = 0x534F5043; // 'CPOS'
static constexpr uint32_t CHUNK_CNML = 0x4C4D4E43; // 'CNML'
static constexpr uint32_t CHUNK_CINX = 0x584E4943; // 'CINX'
static constexpr uint32_t CHUNK_M3SI = 0x49534D33; // 'M3SI'
static constexpr uint32_t CHUNK_M3ST = 0x54534D33; // 'M3ST'
static constexpr uint32_t CHUNK_M3VS = 0x53564D33; // 'M3VS'
static constexpr uint32_t CHUNK_M3PT = 0x54504D33; // 'M3PT'

M3Loader::M3Loader(BufferWrapper& data)
	: data(data) {
}

std::string M3Loader::fourCCToString(uint32_t chunkID) {
	char chars[4];
	chars[0] = static_cast<char>(chunkID & 0xFF);
	chars[1] = static_cast<char>((chunkID >> 8) & 0xFF);
	chars[2] = static_cast<char>((chunkID >> 16) & 0xFF);
	chars[3] = static_cast<char>((chunkID >> 24) & 0xFF);
	return std::string(chars, 4);
}

void M3Loader::load() {
	// Prevent multiple loading of the same M3.
	if (this->isLoaded)
		return;

	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const uint32_t propertyA = this->data.readUInt32LE();
		const uint32_t propertyB = this->data.readUInt32LE();

		// chunkSize is data size after 16-byte header (id + size + propA + propB)
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		logging::write(std::format("M3Loader: Processing chunk {} ({} bytes)", fourCCToString(chunkID), chunkSize));

		switch (chunkID) {
			case CHUNK_M3DT: this->parseChunk_M3DT(chunkSize); break;
			case CHUNK_M3SI: this->parseChunk_M3SI(chunkSize); break;
			case CHUNK_MES3: this->parseChunk_MES3(chunkSize); break;
			case CHUNK_M3CL: this->parseChunk_M3CL(chunkSize); break;
		}

		// Ensure that we start at the next chunk exactly.
		if (this->data.offset() != nextChunkPos)
			logging::write(std::format("M3Loader: Warning, chunk {} did not end at expected position ({} != {})", fourCCToString(chunkID), this->data.offset(), nextChunkPos));

		this->data.seek(nextChunkPos);
	}

	this->isLoaded = true;
}

void M3Loader::parseChunk_M3DT(uint32_t chunkSize) {
	this->data.move(chunkSize); // TODO: Skip the chunk data for now.
}

void M3Loader::parseChunk_MES3(uint32_t chunkSize) {
	const size_t endPos = this->data.offset() + chunkSize;

	while (this->data.offset() < endPos) {
		const uint32_t subChunkID = this->data.readUInt32LE();
		const uint32_t subChunkSize = this->data.readUInt32LE();

		const uint32_t propertyA = this->data.readUInt32LE();
		const uint32_t propertyB = this->data.readUInt32LE();

		const size_t nextSubChunkPos = this->data.offset() + subChunkSize;

		logging::write(std::format("M3Loader: Processing MES3 sub-chunk {} ({} bytes)", fourCCToString(subChunkID), subChunkSize));

		switch (subChunkID) {
			case CHUNK_M3VR: break; // TODO: 0-size chunk, uses properties
			case CHUNK_VPOS: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VNML: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VUV0: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VUV1: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VUV2: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VUV3: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VUV4: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VUV5: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VTAN: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VINX: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VWTS: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VIBP: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VCL0: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VCL1: this->parseBufferChunk(subChunkSize, subChunkID, propertyA, propertyB); break;
			case CHUNK_VSTR: this->parseSubChunk_VSTR(subChunkSize); break;
			case CHUNK_VGEO: this->parseSubChunk_VGEO(propertyA); break;
			case CHUNK_LODS: this->parseSubChunk_LODS(propertyA, propertyB); break;
			case CHUNK_RBAT: this->parseSubChunk_RBAT(propertyA); break;
		}

		// Ensure that we start at the next chunk exactly.
		if (this->data.offset() != nextSubChunkPos)
			logging::write(std::format("M3Loader: Warning, MES3 sub-chunk {} did not end at expected position ({} != {})", fourCCToString(subChunkID), this->data.offset(), nextSubChunkPos));

		this->data.seek(nextSubChunkPos);
	}
}

void M3Loader::parseBufferChunk(uint32_t chunkSize, uint32_t chunkID, uint32_t propertyA, uint32_t propertyB) {
	const std::string chunkName = fourCCToString(chunkID);
	const std::string format = fourCCToString(propertyA);

	switch (chunkID) {
		case CHUNK_VPOS:
		{
			if (format != "3F32")
				throw std::runtime_error(std::format("M3Loader: Unexpected {} format {}", chunkName, format));
			auto floatArray = this->ReadBufferAsFormat(format, chunkSize);
			this->vertices.resize(floatArray.size());
			for (size_t i = 0; i < floatArray.size(); i += 3) {
				this->vertices[i] = floatArray[i];
				this->vertices[i + 2] = floatArray[i + 1] * -1;
				this->vertices[i + 1] = floatArray[i + 2];
			}
			break;
		}
		case CHUNK_VNML:
		{
			if (format != "3F32")
				throw std::runtime_error(std::format("M3Loader: Unexpected {} format {}", chunkName, format));
			auto floatArray = this->ReadBufferAsFormat(format, chunkSize);
			this->normals.resize(floatArray.size());
			for (size_t i = 0; i < floatArray.size(); i += 3) {
				this->normals[i] = floatArray[i];
				this->normals[i + 2] = floatArray[i + 1] * -1;
				this->normals[i + 1] = floatArray[i + 2];
			}
			break;
		}
		case CHUNK_VUV0:
		case CHUNK_VUV1:
		case CHUNK_VUV2:
		case CHUNK_VUV3:
		case CHUNK_VUV4:
		case CHUNK_VUV5:
		{
			if (format != "2F32")
				throw std::runtime_error(std::format("M3Loader: Unexpected {} format {}", chunkName, format));

			auto fixedUVs = this->ReadBufferAsFormat(format, chunkSize);

			if (chunkID == CHUNK_VUV0)
				this->uv = std::move(fixedUVs);
			else if (chunkID == CHUNK_VUV1)
				this->uv1 = std::move(fixedUVs);
			else if (chunkID == CHUNK_VUV2)
				this->uv2 = std::move(fixedUVs);
			else if (chunkID == CHUNK_VUV3)
				this->uv3 = std::move(fixedUVs);
			else if (chunkID == CHUNK_VUV4)
				this->uv4 = std::move(fixedUVs);
			else if (chunkID == CHUNK_VUV5)
				this->uv5 = std::move(fixedUVs);

			break;
		}

		case CHUNK_VTAN:
			if (format != "4F32")
				throw std::runtime_error(std::format("M3Loader: Unexpected {} format {}", chunkName, format));
			this->tangents = this->ReadBufferAsFormat(format, chunkSize);
			break;
		case CHUNK_VINX:
			if (format != "1U16")
				throw std::runtime_error(std::format("M3Loader: Unexpected {} format {}", chunkName, format));
			this->indices = this->ReadBufferAsFormatU16(format, chunkSize);
			break;
		default:
			logging::write(std::format("M3Loader: Unhandled buffer chunk {} with format {} ({} bytes, properties: {}, {})", chunkName, format, chunkSize, propertyA, propertyB));
			this->data.move(chunkSize); // Skip the chunk data for now.
			return;
	}
}

std::vector<float> M3Loader::ReadBufferAsFormat(const std::string& format, uint32_t chunkSize) {
	// TODO: Surely we can just read the data directly into their respective typed arrays? Unless we need to do coordinate conversion...

	if (format == "1F32" || format == "2F32" || format == "3F32" || format == "4F32") {
		const uint32_t floatCount = chunkSize / 4;
		std::vector<float> floatArray(floatCount);
		for (uint32_t i = 0; i < floatCount; i++)
			floatArray[i] = this->data.readFloatLE();
		return floatArray;
	}

	logging::write(std::format("M3Loader: Unsupported buffer format {}", format));
	throw std::runtime_error(std::format("Unsupported format {}", format));
}

std::vector<uint16_t> M3Loader::ReadBufferAsFormatU16(const std::string& format, uint32_t chunkSize) {
	if (format == "1U16") {
		const uint32_t u16Count = chunkSize / 2;
		std::vector<uint16_t> u16Array(u16Count);
		for (uint32_t i = 0; i < u16Count; i++)
			u16Array[i] = this->data.readUInt16LE();
		return u16Array;
	}

	logging::write(std::format("M3Loader: Unsupported buffer format {}", format));
	throw std::runtime_error(std::format("Unsupported format {}", format));
}

void M3Loader::parseSubChunk_VSTR(uint32_t chunkSize) {
	this->ownedStringBlock = std::make_unique<BufferWrapper>(this->data.readBuffer(chunkSize, false));
	this->stringBlock = this->ownedStringBlock.get();
}

void M3Loader::parseSubChunk_VGEO(uint32_t numGeosets) {
	this->geosets.resize(numGeosets);
	for (uint32_t i = 0; i < numGeosets; i++) {
		this->geosets[i] = {
			this->data.readUInt32LE(),
			this->data.readUInt32LE(),
			this->data.readUInt32LE(),
			this->data.readUInt32LE(),
			this->data.readUInt32LE(),
			this->data.readUInt32LE(),
			this->data.readUInt32LE(),
			this->data.readUInt32LE(),
			this->data.readUInt32LE()
		};
	}
}

void M3Loader::parseSubChunk_LODS(uint32_t numLODs, uint32_t numGeosetsPerLOD) {
	this->lodCount = numLODs;
	this->geosetCountPerLOD = numGeosetsPerLOD;
	this->lodLevels.resize(numLODs + 1); // +1 for the base LOD
	for (uint32_t i = 0; i < numLODs + 1; i++) {
		this->lodLevels[i] = {
			this->data.readUInt32LE(),
			this->data.readUInt32LE()
		};
	}
}

void M3Loader::parseSubChunk_RBAT(uint32_t numBatches) {
	this->renderBatches.resize(numBatches);
	for (uint32_t i = 0; i < numBatches; i++) {
		this->renderBatches[i] = {
			this->data.readUInt16LE(),
			this->data.readUInt16LE(),
			this->data.readUInt16LE(),
			this->data.readUInt16LE()
		};
	}
}

void M3Loader::parseChunk_M3SI(uint32_t chunkSize) {
	this->data.move(chunkSize); // TODO: Skip the chunk data for now.
}

void M3Loader::parseChunk_M3CL(uint32_t chunkSize) {
	this->data.move(chunkSize); // TODO: Skip the chunk data for now.
}