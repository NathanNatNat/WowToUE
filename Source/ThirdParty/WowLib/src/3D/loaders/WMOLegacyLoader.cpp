/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
*/

#include "WMOLegacyLoader.h"
#include "LoaderGenerics.h"
#include "../../buffer.h"
#include "../../core.h"
#include "../../casc/listfile.h"
#include "../../mpq/mpq-install.h"

#include <algorithm>
#include <format>
#include <stdexcept>

static constexpr uint32_t CHUNK_MVER = 0x4D564552;
static constexpr uint32_t CHUNK_MOHD = 0x4D4F4844;
static constexpr uint32_t CHUNK_MOTX = 0x4D4F5458;
static constexpr uint32_t CHUNK_MFOG = 0x4D464F47;
static constexpr uint32_t CHUNK_MOMT = 0x4D4F4D54;
static constexpr uint32_t CHUNK_MOPV = 0x4D4F5056;
static constexpr uint32_t CHUNK_MOPT = 0x4D4F5054;
static constexpr uint32_t CHUNK_MOPR = 0x4D4F5052;
static constexpr uint32_t CHUNK_MOGN = 0x4D4F474E;
static constexpr uint32_t CHUNK_MOGI = 0x4D4F4749;
static constexpr uint32_t CHUNK_MODS = 0x4D4F4453;
static constexpr uint32_t CHUNK_MODI = 0x4D4F4449;
static constexpr uint32_t CHUNK_MODN = 0x4D4F444E;
static constexpr uint32_t CHUNK_MODD = 0x4D4F4444;
static constexpr uint32_t CHUNK_GFID = 0x47464944;
static constexpr uint32_t CHUNK_MLIQ = 0x4D4C4951;
static constexpr uint32_t CHUNK_MOCV = 0x4D4F4356;
static constexpr uint32_t CHUNK_MDAL = 0x4D44414C;
static constexpr uint32_t CHUNK_MOGP = 0x4D4F4750;
static constexpr uint32_t CHUNK_MOVI = 0x4D4F5649;
static constexpr uint32_t CHUNK_MOVT = 0x4D4F5654;
static constexpr uint32_t CHUNK_MOTV = 0x4D4F5456;
static constexpr uint32_t CHUNK_MONR = 0x4D4F4E52;
static constexpr uint32_t CHUNK_MOBA = 0x4D4F4241;
static constexpr uint32_t CHUNK_MOPY = 0x4D4F5059;
static constexpr uint32_t CHUNK_MOC2 = 0x4D4F4332;
static constexpr uint32_t CHUNK_MOMO = 0x4F4D4F4D;
static constexpr uint32_t CHUNK_MOGP_ALPHA = 0x5047474D;

static constexpr uint32_t LEGACY_OPTIONAL_CHUNKS[] = {
	CHUNK_MLIQ,  // MLIQ (Liquid)
	CHUNK_MFOG,  // MFOG (Fog)
	CHUNK_MOPV,  // MOPV (Portal Vertices)
	CHUNK_MOPR,  // MOPR (Map Object Portal References)
	CHUNK_MOPT,  // MOPT (Portal Triangles)
	CHUNK_MOCV,  // MOCV (Vertex Colors)
	CHUNK_MDAL,  // MDAL (Ambient Color)
};

bool WMOLegacyLoader::isOptionalChunk(uint32_t chunkID) {
	for (auto id : LEGACY_OPTIONAL_CHUNKS)
		if (id == chunkID)
			return true;
	return false;
}

WMOLegacyLoader::WMOLegacyLoader(BufferWrapper& data, uint32_t fileID, bool renderingOnly)
	: loaded(false), renderingOnly(renderingOnly), data(&data) {
	if (fileID != 0) {
		this->fileDataID = fileID;
		this->fileName = casc::listfile::getByID(fileID).value_or("");
	}
}

WMOLegacyLoader::WMOLegacyLoader(BufferWrapper& data, const std::string& fileName, bool renderingOnly)
	: loaded(false), renderingOnly(renderingOnly), data(&data) {
	this->fileName = fileName;
	auto id = casc::listfile::getByFilename(fileName);
	if (id.has_value())
		this->fileDataID = id.value();
}

void WMOLegacyLoader::load() {
	if (this->loaded)
		return;

	auto& d = *this->data;

	// check for MOMO wrapper (alpha v14 format)
	const uint32_t first_chunk = d.readUInt32LE();
	d.seek(0);

	if (first_chunk == CHUNK_MOMO) {
		_load_alpha_format();
	} else {
		_load_standard_format();
	}

	this->loaded = true;
	this->data = nullptr;
}

// alpha format: MOMO wrapper contains all root data
void WMOLegacyLoader::_load_alpha_format() {
	auto& d = *this->data;

	while (d.remainingBytes() > 0) {
		const uint32_t chunkID = d.readUInt32LE();
		const uint32_t chunkSize = d.readUInt32LE();
		const size_t nextChunkPos = d.offset() + chunkSize;

		if (chunkID == CHUNK_MOMO) {
			// parse chunks inside MOMO
			const size_t momoEnd = d.offset() + chunkSize;
			while (d.offset() < momoEnd) {
				const uint32_t subChunkID = d.readUInt32LE();
				const uint32_t subChunkSize = d.readUInt32LE();
				const size_t subNextPos = d.offset() + subChunkSize;

				if (!this->renderingOnly || !isOptionalChunk(subChunkID))
					handleChunk(subChunkID, d, subChunkSize);

				d.seek(subNextPos);
			}
		} else if (chunkID == CHUNK_MOGP_ALPHA) {
			// alpha has group data inline after MOMO
			_parse_alpha_group(d, chunkSize);
		}

		d.seek(nextChunkPos);
	}
}

// standard format: chunked root file
void WMOLegacyLoader::_load_standard_format() {
	auto& d = *this->data;

	while (d.remainingBytes() > 0) {
		const uint32_t chunkID = d.readUInt32LE();
		const uint32_t chunkSize = d.readUInt32LE();
		const size_t nextChunkPos = d.offset() + chunkSize;

		if (!this->renderingOnly || !isOptionalChunk(chunkID))
			handleChunk(chunkID, d, chunkSize);

		d.seek(nextChunkPos);
	}
}

void WMOLegacyLoader::_parse_alpha_group(BufferWrapper& data, uint32_t chunkSize) {
	// alpha format embeds group data directly
	// for now, store raw for later parsing
	WMOAlphaGroupEntry entry;
	entry.offset = data.offset();
	entry.size = chunkSize;
	this->alphaGroups.push_back(entry);

	data.move(chunkSize);
}

WMOLegacyLoader& WMOLegacyLoader::getGroup(uint32_t index) {
	if (!this->groupsInitialized)
		throw std::runtime_error("Attempted to obtain group from a root WMO.");

	if (index < this->groups.size() && this->groups[index] != nullptr)
		return *this->groups[index];

	// alpha format: groups are inline
	if (this->version == WMO_VER_ALPHA && !this->alphaGroups.empty()) {
		// parse inline group
		if (index >= this->alphaGroups.size())
			throw std::runtime_error("Group not found: " + std::to_string(index));

		// would need to re-read from original data
		throw std::runtime_error("Alpha inline group parsing not yet implemented");
	}

	if (!core::view || !core::view->mpq)
		throw std::runtime_error("MPQ install not available for group loading");

	std::string groupPath;
	if (!this->fileName.empty()) {
		groupPath = this->fileName;
		auto dotPos = groupPath.find(".wmo");
		if (dotPos != std::string::npos) {
			std::string suffix = "_" + std::format("{:03}", index) + ".wmo";
			groupPath.replace(dotPos, 4, suffix);
		}
	}

	if (groupPath.empty())
		throw std::runtime_error("Cannot determine group filename for group " + std::to_string(index));

	auto fileData = core::view->mpq->getFile(groupPath);
	if (!fileData.has_value())
		throw std::runtime_error("WMO group file not found in MPQ: " + groupPath);

	auto ownedBuf = std::make_unique<BufferWrapper>(std::move(fileData.value()));
	auto group = std::make_unique<WMOLegacyLoader>(*ownedBuf, 0u, this->renderingOnly);
	group->version = this->version;
	group->load();

	if (this->groups.size() <= index)
		this->groups.resize(index + 1, nullptr);

	WMOLegacyLoader* groupPtr = group.get();
	this->groups[index] = groupPtr;
	this->ownedGroupBuffers.push_back(std::move(ownedBuf));
	this->ownedGroups.push_back(std::move(group));

	return *groupPtr;
}

void WMOLegacyLoader::handleChunk(uint32_t chunkID, BufferWrapper& data, uint32_t chunkSize) {
	switch (chunkID) {
		case CHUNK_MVER: parse_MVER(data); break;
		case CHUNK_MOHD: parse_MOHD(data, chunkSize); break;
		case CHUNK_MOTX: parse_MOTX(data, chunkSize); break;
		case CHUNK_MFOG: parse_MFOG(data, chunkSize); break;
		case CHUNK_MOMT: parse_MOMT(data, chunkSize); break;
		case CHUNK_MOPV: parse_MOPV(data, chunkSize); break;
		case CHUNK_MOPT: parse_MOPT(data); break;
		case CHUNK_MOPR: parse_MOPR(data, chunkSize); break;
		case CHUNK_MOGN: parse_MOGN(data, chunkSize); break;
		case CHUNK_MOGI: parse_MOGI(data, chunkSize); break;
		case CHUNK_MODS: parse_MODS(data, chunkSize); break;
		case CHUNK_MODI: parse_MODI(data, chunkSize); break;
		case CHUNK_MODN: parse_MODN(data, chunkSize); break;
		case CHUNK_MODD: parse_MODD(data, chunkSize); break;
		case CHUNK_GFID: parse_GFID(data, chunkSize); break;
		case CHUNK_MLIQ: parse_MLIQ(data); break;
		case CHUNK_MOCV: parse_MOCV(data, chunkSize); break;
		case CHUNK_MDAL: parse_MDAL(data); break;
		case CHUNK_MOGP: parse_MOGP(data, chunkSize); break;
		case CHUNK_MOVI: parse_MOVI(data, chunkSize); break;
		case CHUNK_MOVT: parse_MOVT(data, chunkSize); break;
		case CHUNK_MOTV: parse_MOTV(data, chunkSize); break;
		case CHUNK_MONR: parse_MONR(data, chunkSize); break;
		case CHUNK_MOBA: parse_MOBA(data, chunkSize); break;
		case CHUNK_MOPY: parse_MOPY(data, chunkSize); break;
		case CHUNK_MOC2: parse_MOC2(data, chunkSize); break;
		default: break;
	}
}

// MVER (Version)
void WMOLegacyLoader::parse_MVER(BufferWrapper& data) {
	this->version = data.readUInt32LE();
	if (this->version < WMO_VER_ALPHA || this->version > WMO_VER_VANILLA)
		throw std::runtime_error("Unsupported WMO version: " + std::to_string(this->version));
}

// MOHD (Header)
void WMOLegacyLoader::parse_MOHD(BufferWrapper& data, uint32_t chunkSize) {
	if (this->version == WMO_VER_ALPHA) {
		// alpha format has version field and different structure
		const uint32_t ver = data.readUInt32LE(); // embedded version
		(void)ver;
		this->materialCount = data.readUInt32LE();
		this->groupCount = data.readUInt32LE();
		this->portalCount = data.readUInt32LE();
		this->lightCount = data.readUInt32LE();
		this->modelCount = data.readUInt32LE();
		this->doodadCount = data.readUInt32LE();
		this->setCount = data.readUInt32LE();
		this->ambientColor = data.readUInt32LE();
		this->wmoID = data.readUInt32LE();
		// alpha has padding instead of bounding box
		data.move(0x1C); // skip padding
	} else {
		// v16/v17 standard format
		this->materialCount = data.readUInt32LE();
		this->groupCount = data.readUInt32LE();
		this->portalCount = data.readUInt32LE();
		this->lightCount = data.readUInt32LE();
		this->modelCount = data.readUInt32LE();
		this->doodadCount = data.readUInt32LE();
		this->setCount = data.readUInt32LE();
		this->ambientColor = data.readUInt32LE();
		this->wmoID = data.readUInt32LE();
		this->boundingBox1 = data.readFloatLE(3);
		this->boundingBox2 = data.readFloatLE(3);
		this->flags = data.readUInt16LE();
		this->lodCount = data.readUInt16LE();
	}

	this->groups.resize(this->groupCount, nullptr);
	this->groupsInitialized = true;
}

// MOTX (Textures)
void WMOLegacyLoader::parse_MOTX(BufferWrapper& data, uint32_t chunkSize) {
	this->textureNames = ReadStringBlock(data, chunkSize);
}

// MFOG (Fog)
void WMOLegacyLoader::parse_MFOG(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 48;
	this->fogs.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		WMOFog& fog = this->fogs[i];
		fog.flags = data.readUInt32LE();
		fog.position = data.readFloatLE(3);
		fog.radiusSmall = data.readFloatLE();
		fog.radiusLarge = data.readFloatLE();
		fog.fog.end = data.readFloatLE();
		fog.fog.startScalar = data.readFloatLE();
		fog.fog.color = data.readUInt32LE();
		fog.underwaterFog.end = data.readFloatLE();
		fog.underwaterFog.startScalar = data.readFloatLE();
		fog.underwaterFog.color = data.readUInt32LE();
	}
}

// MOMT (Materials)
void WMOLegacyLoader::parse_MOMT(BufferWrapper& data, uint32_t chunkSize) {
	uint32_t entrySize;
	if (this->version == WMO_VER_ALPHA)
		entrySize = 0x40; // alpha has version field
	else
		entrySize = 64;

	const uint32_t count = chunkSize / entrySize;
	this->materials.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		WMOMaterial& mat = this->materials[i];

		if (this->version == WMO_VER_ALPHA) {
			// alpha format
			const uint32_t ver = data.readUInt32LE(); // version per material
			(void)ver;
			mat.flags = data.readUInt32LE();
			mat.shader = data.readUInt32LE();
			mat.blendMode = data.readUInt32LE();
			mat.texture1 = data.readUInt32LE();
			mat.color1 = data.readUInt32LE();
			mat.color1b = data.readUInt32LE();
			mat.texture2 = data.readUInt32LE();
			mat.color2 = data.readUInt32LE();
			mat.groupType = data.readUInt32LE();
			mat.texture3 = data.readUInt32LE();
			mat.color3 = data.readUInt32LE();
			mat.flags3 = data.readUInt32LE();
			mat.runtimeData = {0, 0, 0, 0};
			// skip remaining padding
			data.move(entrySize - 52);
		} else {
			// v16/v17 standard format
			mat.flags = data.readUInt32LE();
			mat.shader = data.readUInt32LE();
			mat.blendMode = data.readUInt32LE();
			mat.texture1 = data.readUInt32LE();
			mat.color1 = data.readUInt32LE();
			mat.color1b = data.readUInt32LE();
			mat.texture2 = data.readUInt32LE();
			mat.color2 = data.readUInt32LE();
			mat.groupType = data.readUInt32LE();
			mat.texture3 = data.readUInt32LE();
			mat.color3 = data.readUInt32LE();
			mat.flags3 = data.readUInt32LE();
			mat.runtimeData = data.readUInt32LE(4);
		}
	}
}

// MOPV (Portal Vertices)
void WMOLegacyLoader::parse_MOPV(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t vertexCount = chunkSize / 12;
	this->portalVertices.resize(vertexCount);
	for (uint32_t i = 0; i < vertexCount; i++)
		this->portalVertices[i] = data.readFloatLE(3);
}

// MOPT (Portal Triangles)
void WMOLegacyLoader::parse_MOPT(BufferWrapper& data) {
	this->portalInfo.resize(this->portalCount);
	for (uint32_t i = 0; i < this->portalCount; i++) {
		WMOPortalInfo& info = this->portalInfo[i];
		info.startVertex = data.readUInt16LE();
		info.count = data.readUInt16LE();
		info.plane = data.readFloatLE(4);
	}
}

// MOPR (Map Object Portal References)
void WMOLegacyLoader::parse_MOPR(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t entryCount = chunkSize / 8;
	this->mopr.resize(entryCount);

	for (uint32_t i = 0; i < entryCount; i++) {
		WMOPortalRef& ref = this->mopr[i];
		ref.portalIndex = data.readUInt16LE();
		ref.groupIndex = data.readUInt16LE();
		ref.side = data.readInt16LE();
		data.move(2); // padding
	}
}

// MOGN (Group Names)
void WMOLegacyLoader::parse_MOGN(BufferWrapper& data, uint32_t chunkSize) {
	this->groupNames = ReadStringBlock(data, chunkSize);
}

// MOGI (Group Info)
void WMOLegacyLoader::parse_MOGI(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 32;
	this->groupInfo.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		WMOGroupInfo& info = this->groupInfo[i];
		info.flags = data.readUInt32LE();
		info.boundingBox1 = data.readFloatLE(3);
		info.boundingBox2 = data.readFloatLE(3);
		info.nameIndex = data.readInt32LE();
	}
}

// MODS (Doodad Sets)
void WMOLegacyLoader::parse_MODS(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 32;
	this->doodadSets.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		WMODoodadSet& set = this->doodadSets[i];
		std::string raw = data.readString(20);
		raw.erase(std::remove(raw.begin(), raw.end(), '\0'), raw.end());
		set.name = raw;
		set.firstInstanceIndex = data.readUInt32LE();
		set.doodadCount = data.readUInt32LE();
		set.unused = data.readUInt32LE();
	}
}

// MODI (Doodad IDs) - modern only, not in legacy
void WMOLegacyLoader::parse_MODI(BufferWrapper& data, uint32_t chunkSize) {
	this->fileDataIDs = data.readUInt32LE(chunkSize / 4);
}

// MODN (Doodad Names)
void WMOLegacyLoader::parse_MODN(BufferWrapper& data, uint32_t chunkSize) {
	this->doodadNames = ReadStringBlock(data, chunkSize);

	// convert MDX references to M2
	for (auto& [ofs, file] : this->doodadNames) {
		std::transform(file.begin(), file.end(), file.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		auto pos = file.find(".mdx");
		if (pos != std::string::npos)
			file.replace(pos, 4, ".m2");
	}
}

// MODD (Doodad Definitions)
void WMOLegacyLoader::parse_MODD(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 40;
	this->doodads.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		WMODoodad& d = this->doodads[i];
		d.offset = data.readUInt24LE();
		d.flags = data.readUInt8();
		d.position = data.readFloatLE(3);
		d.rotation = data.readFloatLE(4);
		d.scale = data.readFloatLE();
		d.color = data.readUInt8(4);
	}
}

// GFID (Group file Data IDs) - modern only
void WMOLegacyLoader::parse_GFID(BufferWrapper& data, uint32_t chunkSize) {
	this->groupIDs = data.readUInt32LE(chunkSize / 4);
}

// MLIQ (Liquid Data)
void WMOLegacyLoader::parse_MLIQ(BufferWrapper& data) {
	const uint32_t liquidVertsX = data.readUInt32LE();
	const uint32_t liquidVertsY = data.readUInt32LE();
	const uint32_t liquidTilesX = data.readUInt32LE();
	const uint32_t liquidTilesY = data.readUInt32LE();
	auto liquidCorner = data.readFloatLE(3);
	const uint16_t liquidMaterialID = data.readUInt16LE();

	const uint32_t vertCount = liquidVertsX * liquidVertsY;
	std::vector<WMOLiquidVertex> liquidVertices(vertCount);

	for (uint32_t i = 0; i < vertCount; i++) {
		liquidVertices[i].data = data.readUInt32LE();
		liquidVertices[i].height = data.readFloatLE();
	}

	const uint32_t tileCount = liquidTilesX * liquidTilesY;
	std::vector<uint8_t> liquidTiles(tileCount);

	for (uint32_t i = 0; i < tileCount; i++)
		liquidTiles[i] = data.readUInt8();

	this->liquid.vertX = liquidVertsX;
	this->liquid.vertY = liquidVertsY;
	this->liquid.tileX = liquidTilesX;
	this->liquid.tileY = liquidTilesY;
	this->liquid.vertices = std::move(liquidVertices);
	this->liquid.tiles = std::move(liquidTiles);
	this->liquid.corner = std::move(liquidCorner);
	this->liquid.materialID = liquidMaterialID;
	this->hasLiquid = true;
}

// MOCV (Vertex Colouring)
void WMOLegacyLoader::parse_MOCV(BufferWrapper& data, uint32_t chunkSize) {
	this->vertexColours.push_back(data.readUInt8(chunkSize));
}

// MDAL (Ambient Color)
void WMOLegacyLoader::parse_MDAL(BufferWrapper& data) {
	this->ambientColor = data.readUInt32LE();
}

// MOGP (Group Header)
void WMOLegacyLoader::parse_MOGP(BufferWrapper& data, uint32_t chunkSize) {
	const size_t endOfs = data.offset() + chunkSize;

	this->nameOfs = data.readUInt32LE();
	this->descOfs = data.readUInt32LE();
	this->groupFlags = data.readUInt32LE();
	this->boundingBox1 = data.readFloatLE(3);
	this->boundingBox2 = data.readFloatLE(3);

	if (this->version == WMO_VER_ALPHA) {
		// alpha uses uint32 for portal fields
		this->ofsPortals = data.readUInt32LE();
		this->numPortals = data.readUInt32LE();
	} else {
		this->ofsPortals = data.readUInt16LE();
		this->numPortals = data.readUInt16LE();
	}

	this->numBatchesA = data.readUInt16LE();
	this->numBatchesB = data.readUInt16LE();
	this->numBatchesC = data.readUInt32LE();

	data.move(4); // unused

	this->liquidType = data.readUInt32LE();
	this->groupID = data.readUInt32LE();

	data.move(8); // unknown

	// read sub-chunks
	while (data.offset() < endOfs) {
		const uint32_t subChunkID = data.readUInt32LE();
		const uint32_t subChunkSize = data.readUInt32LE();
		const size_t nextChunkPos = data.offset() + subChunkSize;

		handleChunk(subChunkID, data, subChunkSize);

		data.seek(nextChunkPos);
	}
}

// MOVI (indices)
void WMOLegacyLoader::parse_MOVI(BufferWrapper& data, uint32_t chunkSize) {
	this->indices = data.readUInt16LE(chunkSize / 2);
}

// MOVT (vertices)
void WMOLegacyLoader::parse_MOVT(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 4;
	this->vertices.resize(count);

	for (uint32_t i = 0; i < count; i += 3) {
		this->vertices[i] = data.readFloatLE();
		this->vertices[i + 2] = data.readFloatLE() * -1;
		this->vertices[i + 1] = data.readFloatLE();
	}
}

// MOTV (UVs)
void WMOLegacyLoader::parse_MOTV(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 4;
	std::vector<float> uv(count);
	for (uint32_t i = 0; i < count; i += 2) {
		uv[i] = data.readFloatLE();
		uv[i + 1] = data.readFloatLE();
	}

	this->uvs.push_back(std::move(uv));
}

// MONR (Normals)
void WMOLegacyLoader::parse_MONR(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 4;
	this->normals.resize(count);

	for (uint32_t i = 0; i < count; i += 3) {
		this->normals[i] = data.readFloatLE();
		this->normals[i + 2] = data.readFloatLE() * -1;
		this->normals[i + 1] = data.readFloatLE();
	}
}

// MOBA (Render Batches)
void WMOLegacyLoader::parse_MOBA(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 24;
	this->renderBatches.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		WMORenderBatch& batch = this->renderBatches[i];
		batch.possibleBox1 = data.readUInt16LE(3);
		batch.possibleBox2 = data.readUInt16LE(3);
		batch.firstFace = data.readUInt32LE();
		batch.numFaces = data.readUInt16LE();
		batch.firstVertex = data.readUInt16LE();
		batch.lastVertex = data.readUInt16LE();
		batch.flags = data.readUInt8();
		batch.materialID = data.readUInt8();
	}
}

// MOPY (Material Info)
void WMOLegacyLoader::parse_MOPY(BufferWrapper& data, uint32_t chunkSize) {
	const uint32_t count = chunkSize / 2;
	this->materialInfo.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		this->materialInfo[i].flags = data.readUInt8();
		this->materialInfo[i].materialID = data.readUInt8();
	}
}

// MOC2 (Colors 2)
void WMOLegacyLoader::parse_MOC2(BufferWrapper& data, uint32_t chunkSize) {
	this->colors2 = data.readUInt8(chunkSize);
}
