#include "MDXLoader.h"
#include "../../buffer.h"

#include <algorithm>
#include <format>
#include <sstream>
#include <stdexcept>

static constexpr uint32_t MAGIC_MDLX = 0x584C444D; // 'MDLX'

static constexpr uint32_t NAME_LENGTH = 0x50;
static constexpr uint32_t FILE_NAME_LENGTH = 0x104;

// mdx version range for alpha wow
static constexpr uint32_t MDX_VER_MIN = 1300;
static constexpr uint32_t MDX_VER_MAX = 1500;

// interpolation types
static constexpr uint32_t INTERP_NONE = 0;
static constexpr uint32_t INTERP_LINEAR = 1;
static constexpr uint32_t INTERP_HERMITE = 2;
static constexpr uint32_t INTERP_BEZIER = 3;

MDXLoader::MDXLoader(BufferWrapper& data)
	: isLoaded(false), version(1300), data(data) {
}

void MDXLoader::load() {
	if (this->isLoaded)
		return;

	BufferWrapper& data = this->data;

	const uint32_t magic = data.readUInt32LE();
	if (magic != MAGIC_MDLX)
		throw std::runtime_error("Invalid MDX magic: 0x" + std::format("{:x}", magic));

	this->_init_model();

	while (data.remainingBytes() > 0) {
		const std::string keyword = this->_read_keyword();
		const uint32_t size = data.readUInt32LE();
		const size_t nextChunkPos = data.offset() + size;

		if (keyword == "VERS") this->parse_VERS();
		else if (keyword == "MODL") this->parse_MODL();
		else if (keyword == "SEQS") this->parse_SEQS();
		else if (keyword == "GLBS") this->parse_GLBS(size);
		else if (keyword == "MTLS") this->parse_MTLS();
		else if (keyword == "TEXS") this->parse_TEXS(size);
		else if (keyword == "GEOS") this->parse_GEOS();
		else if (keyword == "GEOA") this->parse_GEOA();
		else if (keyword == "BONE") this->parse_BONE();
		else if (keyword == "HELP") this->parse_HELP();
		else if (keyword == "ATCH") this->parse_ATCH();
		else if (keyword == "PIVT") this->parse_PIVT(size);
		else if (keyword == "EVTS") this->parse_EVTS();
		else if (keyword == "HTST") this->parse_HTST();
		else if (keyword == "CLID") this->parse_CLID();
		else if (keyword == "PREM") this->parse_PREM();
		else if (keyword == "PRE2") this->parse_PRE2();
		else if (keyword == "CAMS") this->parse_CAMS();
		else if (keyword == "LITE") this->parse_LITE();
		else if (keyword == "TXAN") this->parse_TXAN();
		else if (keyword == "RIBB") this->parse_RIBB();

		data.seek(nextChunkPos);
	}

	this->nodes.clear();
	auto registerNode = [this](MDXNode& node) {
		if (node.objectId.has_value()) {
			uint32_t objId = static_cast<uint32_t>(node.objectId.value());
			if (objId >= this->nodes.size())
				this->nodes.resize(objId + 1, nullptr);
			this->nodes[objId] = &node;
		}
	};
	for (auto& bone : this->bones) registerNode(bone);
	for (auto& helper : this->helpers) registerNode(helper);
	for (auto& attachment : this->attachments) registerNode(attachment);
	for (auto& event : this->eventObjects) registerNode(event);
	for (auto& shape : this->hitTestShapes) registerNode(shape);
	for (auto& emitter : this->particleEmitters) registerNode(emitter);
	for (auto& emitter : this->particleEmitters2) registerNode(emitter);
	for (auto& light : this->lights) registerNode(light);
	for (auto& emitter : this->ribbonEmitters) registerNode(emitter);

	// assign pivot points to nodes
	for (size_t i = 0; i < this->nodes.size(); i++) {
		if (this->nodes[i] && this->nodes[i]->objectId.has_value()) {
			uint32_t objId = static_cast<uint32_t>(this->nodes[i]->objectId.value());
			if (objId < this->pivotPoints.size())
				this->nodes[i]->pivotPoint = this->pivotPoints[objId];
		}
	}

	this->isLoaded = true;
}

void MDXLoader::_init_model() {
	this->version = 1300;
	this->info = {};
	this->info.name = "";
	this->info.animationFile = "";
	this->info.boundsRadius = 0;
	this->info.blendTime = 150;
	this->info.flags = 0;
	this->collision = {};
	this->sequences.clear();
	this->globalSequences.clear();
	this->textures.clear();
	this->materials.clear();
	this->textureAnims.clear();
	this->geosets.clear();
	this->geosetAnims.clear();
	this->bones.clear();
	this->helpers.clear();
	this->attachments.clear();
	this->eventObjects.clear();
	this->particleEmitters.clear();
	this->particleEmitters2.clear();
	this->cameras.clear();
	this->lights.clear();
	this->ribbonEmitters.clear();
	this->hitTestShapes.clear();
	this->pivotPoints.clear();
	this->nodes.clear();
}

std::string MDXLoader::_read_keyword() {
	std::vector<uint8_t> bytes = this->data.readUInt8(4);
	return std::string(bytes.begin(), bytes.end());
}

void MDXLoader::_expect_keyword(const std::string& expected, const std::string& error_msg) {
	const std::string keyword = this->_read_keyword();
	if (keyword != expected)
		throw std::runtime_error(error_msg + " (got " + keyword + ")");
}

std::string MDXLoader::_read_string(uint32_t length) {
	std::string str;
	for (uint32_t i = 0; i < length; i++) {
		const uint8_t c = this->data.readUInt8();
		if (c != 0)
			str += static_cast<char>(c);
	}
	return str;
}

void MDXLoader::_read_extent(MDXExtent& obj) {
	obj.boundsRadius = this->data.readFloatLE();
	obj.minExtent = this->data.readFloatLE(3);
	obj.maxExtent = this->data.readFloatLE(3);
}

MDXAnimVector MDXLoader::_read_anim_vector(const std::string& type) {
	BufferWrapper& data = this->data;

	const uint32_t count = data.readUInt32LE();
	const uint32_t lineType = data.readUInt32LE();
	const int32_t globalSeqIdRaw = data.readInt32LE();

	MDXAnimVector result;
	result.lineType = lineType;
	result.globalSeqId = (globalSeqIdRaw == -1) ? std::nullopt : std::optional<int32_t>(globalSeqIdRaw);

	auto readValue = [&]() -> MDXAnimValue {
		if (type == "float1") {
			return data.readFloatLE();
		} else if (type == "float3") {
			return data.readFloatLE(3);
		} else if (type == "float4") {
			return data.readFloatLE(4);
		} else if (type == "int1") {
			return data.readInt32LE();
		} else {
			throw std::runtime_error("Unknown anim vector type: " + type);
		}
	};

	for (uint32_t i = 0; i < count; i++) {
		MDXAnimKey key;
		key.frame = data.readInt32LE();
		key.value = readValue();

		if (lineType == INTERP_HERMITE || lineType == INTERP_BEZIER) {
			key.inTan = readValue();
			key.outTan = readValue();
		}

		result.keys.push_back(std::move(key));
	}

	return result;
}

void MDXLoader::_read_node(MDXNode& node) {
	BufferWrapper& data = this->data;
	const size_t startPos = data.offset();
	const uint32_t size = data.readUInt32LE();

	node.name = this->_read_string(NAME_LENGTH);
	int32_t objectIdRaw = data.readInt32LE();
	int32_t parentRaw = data.readInt32LE();
	node.flags = data.readUInt32LE();

	node.objectId = (objectIdRaw == -1) ? std::nullopt : std::optional<int32_t>(objectIdRaw);
	node.parent = (parentRaw == -1) ? std::nullopt : std::optional<int32_t>(parentRaw);

	while (data.offset() < startPos + size) {
		const std::string keyword = this->_read_keyword();
		if (keyword == "KGTR") {
			node.translation = this->_read_anim_vector("float3");
		} else if (keyword == "KGRT") {
			node.rotation = this->_read_anim_vector("float4");
		} else if (keyword == "KGSC") {
			node.scale = this->_read_anim_vector("float3");
		} else {
			throw std::runtime_error("Unknown node chunk: " + keyword);
		}
	}

}

void MDXLoader::parse_VERS() {
	this->version = this->data.readUInt32LE();
	if (this->version < MDX_VER_MIN || this->version > MDX_VER_MAX)
		throw std::runtime_error("Unsupported MDX version: " + std::to_string(this->version));
}

void MDXLoader::parse_MODL() {
	this->info.name = this->_read_string(NAME_LENGTH);
	this->info.animationFile = this->_read_string(FILE_NAME_LENGTH);
	this->_read_extent(this->info);
	this->info.blendTime = this->data.readUInt32LE();
	this->info.flags = this->data.readUInt8();
}

void MDXLoader::parse_SEQS() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		MDXSequence seq;
		seq.name = this->_read_string(NAME_LENGTH);
		seq.interval = { this->data.readUInt32LE(), this->data.readUInt32LE() };
		seq.moveSpeed = this->data.readFloatLE();
		seq.nonLooping = this->data.readInt32LE() > 0;
		this->_read_extent(seq);
		seq.frequency = this->data.readFloatLE();
		seq.replay = { this->data.readUInt32LE(), this->data.readUInt32LE() };
		seq.blendTime = this->data.readInt32LE();

		this->sequences.push_back(std::move(seq));
	}
}

void MDXLoader::parse_GLBS(uint32_t size) {
	const uint32_t count = size / 4;
	for (uint32_t i = 0; i < count; i++)
		this->globalSequences.push_back(this->data.readUInt32LE());
}

void MDXLoader::parse_MTLS() {
	const uint32_t count = this->data.readUInt32LE();
	this->data.readUInt32LE(); // unused

	for (uint32_t i = 0; i < count; i++) {
		MDXMaterial material;

		this->data.readUInt32LE(); // material size
		material.priorityPlane = this->data.readInt32LE();

		const uint32_t layerCount = this->data.readUInt32LE();

		for (uint32_t j = 0; j < layerCount; j++) {
			const size_t startPos = this->data.offset();
			const uint32_t layerSize = this->data.readUInt32LE();

			MDXMaterialLayer layer;
			layer.filterMode = this->data.readInt32LE();
			layer.shading = this->data.readInt32LE();
			layer.textureId = this->data.readInt32LE();
			int32_t tVertexAnimIdRaw = this->data.readInt32LE();
			layer.coordId = this->data.readInt32LE();
			layer.alpha = this->data.readFloatLE();

			layer.tVertexAnimId = (tVertexAnimIdRaw == -1) ? std::nullopt : std::optional<int32_t>(tVertexAnimIdRaw);

			while (this->data.offset() < startPos + layerSize) {
				const std::string keyword = this->_read_keyword();
				if (keyword == "KMTA") {
					layer.alphaAnim = this->_read_anim_vector("float1");
				} else if (keyword == "KMTF") {
					layer.textureIdAnim = this->_read_anim_vector("int1");
				} else {
					throw std::runtime_error("Unknown layer chunk: " + keyword);
				}
			}

			material.layers.push_back(std::move(layer));
		}

		this->materials.push_back(std::move(material));
	}
}

void MDXLoader::parse_TEXS(uint32_t size) {
	const size_t startPos = this->data.offset();

	while (this->data.offset() < startPos + size) {
		MDXTexture texture;
		texture.replaceableId = this->data.readInt32LE();
		texture.image = this->_read_string(FILE_NAME_LENGTH);
		texture.flags = this->data.readInt32LE();

		this->textures.push_back(std::move(texture));
	}
}

void MDXLoader::parse_GEOS() {
	if (this->version == 1500)
		this->_parse_geosets_v1500();
	else
		this->_parse_geosets_v1300();
}

void MDXLoader::parse_GEOA() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t size = this->data.readUInt32LE();

		MDXGeosetAnim anim;
		int32_t geosetIdRaw = this->data.readInt32LE();
		anim.alpha = this->data.readFloatLE();
		anim.color = this->data.readFloatLE(3);
		anim.flags = this->data.readInt32LE();

		anim.geosetId = (geosetIdRaw == -1) ? std::nullopt : std::optional<int32_t>(geosetIdRaw);

		while (this->data.offset() < startPos + size) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KGAO") {
				anim.alphaAnim = this->_read_anim_vector("float1");
			} else if (keyword == "KGAC") {
				anim.colorAnim = this->_read_anim_vector("float3");
			} else {
				throw std::runtime_error("Unknown geoset anim chunk: " + keyword);
			}
		}

		this->geosetAnims.push_back(std::move(anim));
	}
}

void MDXLoader::parse_BONE() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		MDXBone bone;
		this->_read_node(bone);

		int32_t geosetIdRaw = this->data.readInt32LE();
		int32_t geosetAnimIdRaw = this->data.readInt32LE();

		bone.geosetId = (geosetIdRaw == -1) ? std::nullopt : std::optional<int32_t>(geosetIdRaw);
		bone.geosetAnimId = (geosetAnimIdRaw == -1) ? std::nullopt : std::optional<int32_t>(geosetAnimIdRaw);

		this->bones.push_back(std::move(bone));
	}
}

void MDXLoader::parse_HELP() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		MDXNode helper;
		this->_read_node(helper);
		this->helpers.push_back(std::move(helper));
	}
}

void MDXLoader::parse_ATCH() {
	const uint32_t count = this->data.readUInt32LE();
	this->data.readUInt32LE(); // unused

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t attachmentSize = this->data.readUInt32LE(); // size

		MDXAttachment attachment;
		this->_read_node(attachment);

		attachment.attachmentId = this->data.readInt32LE();
		this->data.readUInt8(); // padding
		attachment.path = this->_read_string(FILE_NAME_LENGTH);
		attachment.visibility = 1.0f;

		if (this->data.offset() < startPos + attachmentSize) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KVIS")
				attachment.visibilityAnim = this->_read_anim_vector("float1");
		}

		this->attachments.push_back(std::move(attachment));
	}
}

void MDXLoader::parse_PIVT(uint32_t size) {
	const uint32_t count = size / 12;
	for (uint32_t i = 0; i < count; i++)
		this->pivotPoints.push_back(this->data.readFloatLE(3));
}

void MDXLoader::parse_EVTS() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		this->data.readUInt32LE(); // size

		MDXEventObject event;
		this->_read_node(event);

		// check for KEVT
		const std::string keyword = this->_read_keyword();
		if (keyword == "KEVT") {
			const uint32_t trackCount = this->data.readUInt32LE();
			event.globalSeqId = this->data.readInt32LE();
			for (uint32_t j = 0; j < trackCount; j++)
				event.eventTrack.push_back(this->data.readInt32LE());
		}

		this->eventObjects.push_back(std::move(event));
	}
}

void MDXLoader::parse_HTST() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		this->data.readUInt32LE(); // size

		MDXHitTestShape shape;
		this->_read_node(shape);

		shape.shapeType = this->data.readUInt8();

		switch (shape.shapeType) {
			case 0: // box
				shape.vertices = this->data.readFloatLE(6);
				break;
			case 1: // cylinder
				shape.vertices = this->data.readFloatLE(5);
				break;
			case 2: // sphere
				shape.vertices = this->data.readFloatLE(4);
				break;
			case 3: // plane
				shape.vertices = this->data.readFloatLE(2);
				break;
		}

		this->hitTestShapes.push_back(std::move(shape));
	}
}

void MDXLoader::parse_CLID() {
	this->_expect_keyword("VRTX", "Invalid collision format");
	this->collision.vertices = this->data.readFloatLE(this->data.readUInt32LE() * 3);

	this->_expect_keyword("TRI ", "Invalid collision format");
	this->collision.triIndices = this->data.readUInt16LE(this->data.readUInt32LE());

	this->_expect_keyword("NRMS", "Invalid collision format");
	this->collision.facetNormals = this->data.readFloatLE(this->data.readUInt32LE() * 3);
}

void MDXLoader::parse_PREM() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t size = this->data.readUInt32LE();

		MDXParticleEmitter emitter;
		this->_read_node(emitter);

		emitter.emissionRate = this->data.readFloatLE();
		emitter.gravity = this->data.readFloatLE();
		emitter.longitude = this->data.readFloatLE();
		emitter.latitude = this->data.readFloatLE();
		emitter.path = this->_read_string(FILE_NAME_LENGTH);
		emitter.lifeSpan = this->data.readFloatLE();
		emitter.initVelocity = this->data.readFloatLE();
		emitter.visibility = 1.0f;

		while (this->data.offset() < startPos + size) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KVIS") emitter.visibilityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KPEE") emitter.emissionRateAnim = this->_read_anim_vector("float1");
			else if (keyword == "KPEG") emitter.gravityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KPLN") emitter.longitudeAnim = this->_read_anim_vector("float1");
			else if (keyword == "KPLT") emitter.latitudeAnim = this->_read_anim_vector("float1");
			else if (keyword == "KPEL") emitter.lifeSpanAnim = this->_read_anim_vector("float1");
			else if (keyword == "KPES") emitter.initVelocityAnim = this->_read_anim_vector("float1");
			else throw std::runtime_error("Unknown particle emitter chunk: " + keyword);
		}

		this->particleEmitters.push_back(std::move(emitter));
	}
}

void MDXLoader::parse_PRE2() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t size = this->data.readUInt32LE();

		MDXParticleEmitter2 emitter;
		this->_read_node(emitter);

		this->data.readUInt32LE(); // content size
		emitter.emitterType = this->data.readInt32LE();
		emitter.speed = this->data.readFloatLE();
		emitter.variation = this->data.readFloatLE();
		emitter.latitude = this->data.readFloatLE();
		emitter.longitude = this->data.readFloatLE();
		emitter.gravity = this->data.readFloatLE();
		emitter.zSource = this->data.readFloatLE();
		emitter.lifeSpan = this->data.readFloatLE();
		emitter.emissionRate = this->data.readFloatLE();
		emitter.length = this->data.readFloatLE();
		emitter.width = this->data.readFloatLE();
		emitter.rows = this->data.readInt32LE();
		emitter.columns = this->data.readInt32LE();
		emitter.particleType = this->data.readInt32LE() + 1;
		emitter.tailLength = this->data.readFloatLE();
		emitter.middleTime = this->data.readFloatLE();
		emitter.segmentColor = {
			this->data.readFloatLE(3),
			this->data.readFloatLE(3),
			this->data.readFloatLE(3)
		};
		emitter.alpha = this->data.readUInt8(3);
		emitter.particleScaling = this->data.readFloatLE(3);
		emitter.lifeSpanUVAnim = this->data.readFloatLE(3);
		emitter.decayUVAnim = this->data.readFloatLE(3);
		emitter.tailUVAnim = this->data.readFloatLE(3);
		emitter.tailDecayUVAnim = this->data.readFloatLE(3);
		emitter.blendMode = this->data.readInt32LE();
		int32_t textureIdRaw = this->data.readInt32LE();
		emitter.priorityPlane = this->data.readInt32LE();
		emitter.replaceableId = this->data.readInt32LE();
		emitter.geometryModel = this->_read_string(FILE_NAME_LENGTH);
		emitter.recursionModel = this->_read_string(FILE_NAME_LENGTH);
		emitter.twinkleFps = this->data.readFloatLE();
		emitter.twinkleOnOff = this->data.readFloatLE();
		emitter.twinkleScale = this->data.readFloatLE(2);
		emitter.ivelScale = this->data.readFloatLE();
		emitter.tumble = this->data.readFloatLE(6);
		emitter.drag = this->data.readFloatLE();
		emitter.spin = this->data.readFloatLE();
		emitter.windVector = this->data.readFloatLE(3);
		emitter.windTime = this->data.readFloatLE();
		emitter.followSpeed = this->data.readFloatLE(2);
		emitter.followScale = this->data.readFloatLE(2);

		const uint32_t splineCount = this->data.readUInt32LE();
		for (uint32_t j = 0; j < splineCount; j++)
			emitter.splines.push_back(this->data.readFloatLE(3));

		emitter.squirt = this->data.readInt32LE() > 0;
		emitter.visibility = 1.0f;

		emitter.textureId = (textureIdRaw == -1) ? std::nullopt : std::optional<int32_t>(textureIdRaw);

		while (this->data.offset() < startPos + size) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KP2S") emitter.speedAnim = this->_read_anim_vector("float1");
			else if (keyword == "KP2R") emitter.variationAnim = this->_read_anim_vector("float1");
			else if (keyword == "KP2G") emitter.gravityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KP2W") emitter.widthAnim = this->_read_anim_vector("float1");
			else if (keyword == "KP2N") emitter.lengthAnim = this->_read_anim_vector("float1");
			else if (keyword == "KVIS") emitter.visibilityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KP2E") emitter.emissionRateAnim = this->_read_anim_vector("float1");
			else if (keyword == "KP2L") emitter.latitudeAnim = this->_read_anim_vector("float1");
			else if (keyword == "KPLN") emitter.longitudeAnim = this->_read_anim_vector("float1");
			else if (keyword == "KLIF") emitter.lifeSpanAnim = this->_read_anim_vector("float1");
			else if (keyword == "KP2Z") emitter.zSourceAnim = this->_read_anim_vector("float1");
			else throw std::runtime_error("Unknown particle emitter2 chunk: " + keyword);
		}

		this->particleEmitters2.push_back(std::move(emitter));
	}
}

void MDXLoader::parse_CAMS() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t size = this->data.readUInt32LE();

		MDXCamera camera;
		camera.name = this->_read_string(NAME_LENGTH);
		camera.pivot = this->data.readFloatLE(3);
		camera.fieldOfView = this->data.readFloatLE();
		camera.farClip = this->data.readFloatLE();
		camera.nearClip = this->data.readFloatLE();
		camera.targetPosition = this->data.readFloatLE(3);
		camera.visibility = 1.0f;

		while (this->data.offset() < startPos + size) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KVIS") camera.visibilityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KCTR") camera.translation = this->_read_anim_vector("float3");
			else if (keyword == "KTTR") camera.targetTranslation = this->_read_anim_vector("float3");
			else if (keyword == "KCRL") camera.rotationAnim = this->_read_anim_vector("float1");
			else throw std::runtime_error("Unknown camera chunk: " + keyword);
		}

		this->cameras.push_back(std::move(camera));
	}
}

void MDXLoader::parse_LITE() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t size = this->data.readUInt32LE();

		MDXLight light;
		this->_read_node(light);

		light.lightType = this->data.readInt32LE();
		light.attenuationStart = this->data.readFloatLE();
		light.attenuationEnd = this->data.readFloatLE();
		light.color = this->data.readFloatLE(3);
		light.intensity = this->data.readFloatLE();
		light.ambColor = this->data.readFloatLE(3);
		light.ambIntensity = this->data.readFloatLE();
		light.visibility = 1.0f;

		while (this->data.offset() < startPos + size) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KLAS") light.attenuationStartAnim = this->_read_anim_vector("int1");
			else if (keyword == "KLAE") light.attenuationEndAnim = this->_read_anim_vector("int1");
			else if (keyword == "KLAC") light.colorAnim = this->_read_anim_vector("float3");
			else if (keyword == "KLAI") light.intensityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KLBC") light.ambColorAnim = this->_read_anim_vector("float3");
			else if (keyword == "KLBI") light.ambIntensityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KVIS") light.visibilityAnim = this->_read_anim_vector("float1");
			else throw std::runtime_error("Unknown light chunk: " + keyword);
		}

		this->lights.push_back(std::move(light));
	}
}

void MDXLoader::parse_TXAN() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t size = this->data.readUInt32LE();

		MDXTextureAnim anim;

		while (this->data.offset() < startPos + size) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KTAT") anim.translation = this->_read_anim_vector("float3");
			else if (keyword == "KTAR") anim.rotation = this->_read_anim_vector("float4");
			else if (keyword == "KTAS") anim.scale = this->_read_anim_vector("float3");
			else throw std::runtime_error("Unknown texture anim chunk: " + keyword);
		}

		this->textureAnims.push_back(std::move(anim));
	}
}

void MDXLoader::parse_RIBB() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		const size_t startPos = this->data.offset();
		const uint32_t size = this->data.readUInt32LE();

		MDXRibbonEmitter emitter;
		this->_read_node(emitter);

		this->data.readUInt32LE(); // content size
		emitter.heightAbove = this->data.readFloatLE();
		emitter.heightBelow = this->data.readFloatLE();
		emitter.alpha = this->data.readFloatLE();
		emitter.color = this->data.readFloatLE(3);
		emitter.lifeSpan = this->data.readFloatLE();
		emitter.textureSlot = this->data.readInt32LE();
		emitter.edgesPerSec = this->data.readInt32LE();
		emitter.rows = this->data.readInt32LE();
		emitter.columns = this->data.readInt32LE();
		emitter.materialId = this->data.readInt32LE();
		emitter.gravity = this->data.readFloatLE();
		emitter.visibility = 1.0f;

		while (this->data.offset() < startPos + size) {
			const std::string keyword = this->_read_keyword();
			if (keyword == "KVIS") emitter.visibilityAnim = this->_read_anim_vector("float1");
			else if (keyword == "KRHA") emitter.heightAboveAnim = this->_read_anim_vector("float1");
			else if (keyword == "KRHB") emitter.heightBelowAnim = this->_read_anim_vector("float1");
			else if (keyword == "KRAL") emitter.alphaAnim = this->_read_anim_vector("float1");
			else if (keyword == "KRTX") emitter.textureSlotAnim = this->_read_anim_vector("int1");
			else if (keyword == "KRCO") emitter.colorAnim = this->_read_anim_vector("float3");
			else throw std::runtime_error("Unknown ribbon emitter chunk: " + keyword);
		}

		this->ribbonEmitters.push_back(std::move(emitter));
	}
}

// geoset parsing
void MDXLoader::_parse_geosets_v1300() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		MDXGeoset geoset;

		this->data.readUInt32LE(); // geoset size

		this->_expect_keyword("VRTX", "Invalid geoset format");
		geoset.vertices = this->data.readFloatLE(this->data.readUInt32LE() * 3);

		this->_expect_keyword("NRMS", "Invalid geoset format");
		geoset.normals = this->data.readFloatLE(this->data.readUInt32LE() * 3);

		// check for UVAS
		const std::string keyword = this->_read_keyword();
		if (keyword == "UVAS") {
			const uint32_t texChunkCount = this->data.readUInt32LE();
			const size_t vertCount = geoset.vertices.size() / 3;
			for (uint32_t j = 0; j < texChunkCount; j++)
				geoset.tVertices.push_back(this->data.readFloatLE(static_cast<uint32_t>(vertCount * 2)));
		} else {
			this->data.move(-4);
		}

		this->_expect_keyword("PTYP", "Invalid geoset format");
		const uint32_t primCount = this->data.readUInt32LE();
		for (uint32_t j = 0; j < primCount; j++) {
			if (this->data.readUInt8() != 4)
				throw std::runtime_error("Invalid primitive type");
		}

		this->_expect_keyword("PCNT", "Invalid geoset format");
		this->data.move(this->data.readUInt32LE() * 4); // faceGroups

		this->_expect_keyword("PVTX", "Invalid geoset format");
		geoset.faces = this->data.readUInt16LE(this->data.readUInt32LE());

		this->_expect_keyword("GNDX", "Invalid geoset format");
		geoset.vertexGroup = this->data.readUInt8(this->data.readUInt32LE());

		this->_expect_keyword("MTGC", "Invalid geoset format");
		const uint32_t groupCount = this->data.readUInt32LE();
		geoset.groups.resize(groupCount);
		for (uint32_t j = 0; j < groupCount; j++)
			geoset.groups[j].resize(this->data.readUInt32LE());

		this->_expect_keyword("MATS", "Invalid geoset format");
		const uint32_t totalGroupCount = this->data.readUInt32LE();
		uint32_t groupIndex = 0, groupCounter = 0;
		for (uint32_t j = 0; j < totalGroupCount; j++) {
			if (groupIndex >= geoset.groups[groupCounter].size()) {
				groupIndex = 0;
				groupCounter++;
			}
			geoset.groups[groupCounter][groupIndex++] = this->data.readInt32LE();
		}

		this->_expect_keyword("BIDX", "Invalid geoset format");
		this->data.move(this->data.readUInt32LE() * 4); // bone indices

		this->_expect_keyword("BWGT", "Invalid geoset format");
		this->data.move(this->data.readUInt32LE() * 4); // bone weights

		geoset.materialId = this->data.readInt32LE();
		geoset.selectionGroup = this->data.readInt32LE();
		geoset.flags = this->data.readInt32LE();
		this->_read_extent(geoset);

		const uint32_t animCount = this->data.readUInt32LE();
		for (uint32_t j = 0; j < animCount; j++) {
			MDXGeosetAnimExtent anim;
			this->_read_extent(anim);
			geoset.anims.push_back(std::move(anim));
		}

		this->geosets.push_back(std::move(geoset));
	}
}

void MDXLoader::_parse_geosets_v1500() {
	const uint32_t count = this->data.readUInt32LE();

	for (uint32_t i = 0; i < count; i++) {
		MDXGeoset geoset;

		geoset.materialId = this->data.readInt32LE();
		this->data.readFloatLE(3); // bounds center
		geoset.boundsRadius = this->data.readFloatLE();
		geoset.selectionGroup = this->data.readInt32LE();
		this->data.readInt32LE(); // geoset index
		geoset.flags = this->data.readInt32LE();

		this->_expect_keyword("PVTX", "Invalid geoset format");
		const uint32_t vertexCount = this->data.readUInt32LE();
		this->_expect_keyword("PTYP", "Invalid geoset format");
		this->data.readInt32LE(); // primitive type count
		this->_expect_keyword("PVTX", "Invalid geoset format");
		this->data.readInt32LE(); // primitive vertex count
		this->data.move(8); // padding

		geoset.vertices.resize(vertexCount * 3, 0.0f);
		geoset.normals.resize(vertexCount * 3, 0.0f);
		geoset.vertexGroup.resize(vertexCount, 0);
		geoset.tVertices.push_back(std::vector<float>(vertexCount * 2, 0.0f));

		this->geosets.push_back(std::move(geoset));
	}

	// vertex data
	for (uint32_t i = 0; i < count; i++) {
		MDXGeoset& geoset = this->geosets[i];
		const size_t vertexCount = geoset.vertices.size() / 3;
		std::vector<std::string> boneLookup;

		for (size_t j = 0; j < vertexCount; j++) {
			std::vector<float> pos = this->data.readFloatLE(3);
			geoset.vertices[j * 3] = pos[0];
			geoset.vertices[j * 3 + 1] = pos[1];
			geoset.vertices[j * 3 + 2] = pos[2];

			this->data.readUInt32LE(); // bone weights
			std::vector<uint8_t> boneIndicesVec = this->data.readUInt8(4);
			std::string boneIndices = std::to_string(boneIndicesVec[0]) + "," +
			                          std::to_string(boneIndicesVec[1]) + "," +
			                          std::to_string(boneIndicesVec[2]) + "," +
			                          std::to_string(boneIndicesVec[3]);

			std::vector<float> normal = this->data.readFloatLE(3);
			geoset.normals[j * 3] = normal[0];
			geoset.normals[j * 3 + 1] = normal[1];
			geoset.normals[j * 3 + 2] = normal[2];

			std::vector<float> uv = this->data.readFloatLE(2);
			geoset.tVertices[0][j * 2] = uv[0];
			geoset.tVertices[0][j * 2 + 1] = uv[1];
			this->data.move(8); // unused

			auto it = std::find(boneLookup.begin(), boneLookup.end(), boneIndices);
			int idx;
			if (it == boneLookup.end()) {
				idx = static_cast<int>(boneLookup.size());
				boneLookup.push_back(boneIndices);
			} else {
				idx = static_cast<int>(std::distance(boneLookup.begin(), it));
			}
			geoset.vertexGroup[j] = static_cast<uint8_t>(idx);
		}

		geoset.groups.clear();
		for (const std::string& b : boneLookup) {
			std::string trimmed = b;
			while (trimmed.size() >= 2 && trimmed.substr(trimmed.size() - 2) == ",0")
				trimmed = trimmed.substr(0, trimmed.size() - 2);

			std::vector<int32_t> group;
			std::istringstream ss(trimmed);
			std::string token;
			while (std::getline(ss, token, ',')) {
				group.push_back(std::stoi(token));
			}
			geoset.groups.push_back(std::move(group));
		}

		this->data.readInt32LE(); // primitive type
		this->data.readInt32LE(); // unknown

		const uint16_t numPrimVerts = this->data.readUInt16LE();
		this->data.readUInt16LE(); // min vertex
		this->data.readUInt16LE(); // max vertex
		this->data.readUInt16LE(); // padding

		geoset.faces = this->data.readUInt16LE(numPrimVerts);

		if (numPrimVerts % 8)
			this->data.move(2 * (8 - numPrimVerts % 8)); // padding
	}
}
