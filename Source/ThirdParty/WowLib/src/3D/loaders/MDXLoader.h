/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
*/
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

class BufferWrapper;

// -----------------------------------------------------------------------
// MDX animation vector value: float, vector<float>, or int32_t
// -----------------------------------------------------------------------
using MDXAnimValue = std::variant<float, std::vector<float>, int32_t>;

// -----------------------------------------------------------------------
// MDX animation key
// -----------------------------------------------------------------------
struct MDXAnimKey {
	int32_t frame;
	MDXAnimValue value;
	std::optional<MDXAnimValue> inTan;
	std::optional<MDXAnimValue> outTan;
};

// -----------------------------------------------------------------------
// MDX animation vector result
// -----------------------------------------------------------------------
struct MDXAnimVector {
	uint32_t lineType;
	std::optional<int32_t> globalSeqId;
	std::vector<MDXAnimKey> keys;
};

// -----------------------------------------------------------------------
// MDX extent info (boundsRadius, minExtent, maxExtent)
// -----------------------------------------------------------------------
struct MDXExtent {
	float boundsRadius = 0.0f;
	std::vector<float> minExtent;
	std::vector<float> maxExtent;
};

// -----------------------------------------------------------------------
// MDX model info
// -----------------------------------------------------------------------
struct MDXModelInfo : MDXExtent {
	std::string name;
	std::string animationFile;
	uint32_t blendTime = 150;
	uint32_t flags = 0;
};

// -----------------------------------------------------------------------
// MDX collision
// -----------------------------------------------------------------------
struct MDXCollision {
	std::vector<float> vertices;
	std::vector<uint16_t> triIndices;
	std::vector<float> facetNormals;
};

// -----------------------------------------------------------------------
// MDX sequence
// -----------------------------------------------------------------------
struct MDXSequence : MDXExtent {
	std::string name;
	std::vector<uint32_t> interval; // [start, end]
	float moveSpeed = 0.0f;
	bool nonLooping = false;
	float frequency = 0.0f;
	std::vector<uint32_t> replay; // [start, end]
	int32_t blendTime = 0;
};

// -----------------------------------------------------------------------
// MDX texture
// -----------------------------------------------------------------------
struct MDXTexture {
	int32_t replaceableId = 0;
	std::string image;
	int32_t flags = 0;
};

// -----------------------------------------------------------------------
// MDX material layer
// -----------------------------------------------------------------------
struct MDXMaterialLayer {
	int32_t filterMode = 0;
	int32_t shading = 0;
	int32_t textureId = 0;
	std::optional<int32_t> tVertexAnimId;
	int32_t coordId = 0;
	float alpha = 0.0f;
	std::optional<MDXAnimVector> alphaAnim;
	std::optional<MDXAnimVector> textureIdAnim;
};

// -----------------------------------------------------------------------
// MDX material
// -----------------------------------------------------------------------
struct MDXMaterial {
	int32_t priorityPlane = 0;
	std::vector<MDXMaterialLayer> layers;
};

// -----------------------------------------------------------------------
// MDX node (shared by bones, helpers, attachments, etc.)
// -----------------------------------------------------------------------
struct MDXNode {
	std::string name;
	std::optional<int32_t> objectId;
	std::optional<int32_t> parent;
	uint32_t flags = 0;
	std::optional<MDXAnimVector> translation;
	std::optional<MDXAnimVector> rotation;
	std::optional<MDXAnimVector> scale;
	std::vector<float> pivotPoint;
};

// -----------------------------------------------------------------------
// MDX geoset anim extent
// -----------------------------------------------------------------------
struct MDXGeosetAnimExtent : MDXExtent {};

// -----------------------------------------------------------------------
// MDX geoset
// -----------------------------------------------------------------------
struct MDXGeoset : MDXExtent {
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<std::vector<float>> tVertices;
	std::vector<uint16_t> faces;
	std::vector<uint8_t> vertexGroup;
	std::vector<std::vector<int32_t>> groups;
	int32_t materialId = 0;
	int32_t selectionGroup = 0;
	int32_t flags = 0;
	std::vector<MDXGeosetAnimExtent> anims;
};

// -----------------------------------------------------------------------
// MDX geoset anim
// -----------------------------------------------------------------------
struct MDXGeosetAnim {
	std::optional<int32_t> geosetId;
	float alpha = 0.0f;
	std::vector<float> color;
	int32_t flags = 0;
	std::optional<MDXAnimVector> alphaAnim;
	std::optional<MDXAnimVector> colorAnim;
};

// -----------------------------------------------------------------------
// MDX bone
// -----------------------------------------------------------------------
struct MDXBone : MDXNode {
	std::optional<int32_t> geosetId;
	std::optional<int32_t> geosetAnimId;
};

// -----------------------------------------------------------------------
// MDX attachment
// -----------------------------------------------------------------------
struct MDXAttachment : MDXNode {
	int32_t attachmentId = 0;
	std::string path;
	float visibility = 1.0f;
	std::optional<MDXAnimVector> visibilityAnim;
};

// -----------------------------------------------------------------------
// MDX event object
// -----------------------------------------------------------------------
struct MDXEventObject : MDXNode {
	int32_t globalSeqId = 0;
	std::vector<int32_t> eventTrack;
};

// -----------------------------------------------------------------------
// MDX hit test shape
// -----------------------------------------------------------------------
struct MDXHitTestShape : MDXNode {
	uint8_t shapeType = 0;
	std::vector<float> vertices;
};

// -----------------------------------------------------------------------
// MDX particle emitter
// -----------------------------------------------------------------------
struct MDXParticleEmitter : MDXNode {
	float emissionRate = 0.0f;
	float gravity = 0.0f;
	float longitude = 0.0f;
	float latitude = 0.0f;
	std::string path;
	float lifeSpan = 0.0f;
	float initVelocity = 0.0f;
	float visibility = 1.0f;
	std::optional<MDXAnimVector> visibilityAnim;
	std::optional<MDXAnimVector> emissionRateAnim;
	std::optional<MDXAnimVector> gravityAnim;
	std::optional<MDXAnimVector> longitudeAnim;
	std::optional<MDXAnimVector> latitudeAnim;
	std::optional<MDXAnimVector> lifeSpanAnim;
	std::optional<MDXAnimVector> initVelocityAnim;
};

// -----------------------------------------------------------------------
// MDX particle emitter 2
// -----------------------------------------------------------------------
struct MDXParticleEmitter2 : MDXNode {
	int32_t emitterType = 0;
	float speed = 0.0f;
	float variation = 0.0f;
	float latitude = 0.0f;
	float longitude = 0.0f;
	float gravity = 0.0f;
	float zSource = 0.0f;
	float lifeSpan = 0.0f;
	float emissionRate = 0.0f;
	float length = 0.0f;
	float width = 0.0f;
	int32_t rows = 0;
	int32_t columns = 0;
	int32_t particleType = 0;
	float tailLength = 0.0f;
	float middleTime = 0.0f;
	std::vector<std::vector<float>> segmentColor; // 3 entries of float3
	std::vector<uint8_t> alpha; // 3 bytes
	std::vector<float> particleScaling; // float3
	std::vector<float> lifeSpanUVAnim; // float3
	std::vector<float> decayUVAnim; // float3
	std::vector<float> tailUVAnim; // float3
	std::vector<float> tailDecayUVAnim; // float3
	int32_t blendMode = 0;
	std::optional<int32_t> textureId;
	int32_t priorityPlane = 0;
	int32_t replaceableId = 0;
	std::string geometryModel;
	std::string recursionModel;
	float twinkleFps = 0.0f;
	float twinkleOnOff = 0.0f;
	std::vector<float> twinkleScale; // float2
	float ivelScale = 0.0f;
	std::vector<float> tumble; // float6
	float drag = 0.0f;
	float spin = 0.0f;
	std::vector<float> windVector; // float3
	float windTime = 0.0f;
	std::vector<float> followSpeed; // float2
	std::vector<float> followScale; // float2
	std::vector<std::vector<float>> splines; // array of float3
	bool squirt = false;
	float visibility = 1.0f;
	std::optional<MDXAnimVector> speedAnim;
	std::optional<MDXAnimVector> variationAnim;
	std::optional<MDXAnimVector> gravityAnim;
	std::optional<MDXAnimVector> widthAnim;
	std::optional<MDXAnimVector> lengthAnim;
	std::optional<MDXAnimVector> visibilityAnim;
	std::optional<MDXAnimVector> emissionRateAnim;
	std::optional<MDXAnimVector> latitudeAnim;
	std::optional<MDXAnimVector> longitudeAnim;
	std::optional<MDXAnimVector> lifeSpanAnim;
	std::optional<MDXAnimVector> zSourceAnim;
};

// -----------------------------------------------------------------------
// MDX camera
// -----------------------------------------------------------------------
struct MDXCamera {
	std::string name;
	std::vector<float> pivot;
	float fieldOfView = 0.0f;
	float farClip = 0.0f;
	float nearClip = 0.0f;
	std::vector<float> targetPosition;
	float visibility = 1.0f;
	std::optional<MDXAnimVector> visibilityAnim;
	std::optional<MDXAnimVector> translation;
	std::optional<MDXAnimVector> targetTranslation;
	std::optional<MDXAnimVector> rotationAnim;
};

// -----------------------------------------------------------------------
// MDX light
// -----------------------------------------------------------------------
struct MDXLight : MDXNode {
	int32_t lightType = 0;
	float attenuationStart = 0.0f;
	float attenuationEnd = 0.0f;
	std::vector<float> color;
	float intensity = 0.0f;
	std::vector<float> ambColor;
	float ambIntensity = 0.0f;
	float visibility = 1.0f;
	std::optional<MDXAnimVector> attenuationStartAnim;
	std::optional<MDXAnimVector> attenuationEndAnim;
	std::optional<MDXAnimVector> colorAnim;
	std::optional<MDXAnimVector> intensityAnim;
	std::optional<MDXAnimVector> ambColorAnim;
	std::optional<MDXAnimVector> ambIntensityAnim;
	std::optional<MDXAnimVector> visibilityAnim;
};

// -----------------------------------------------------------------------
// MDX texture anim
// -----------------------------------------------------------------------
struct MDXTextureAnim {
	std::optional<MDXAnimVector> translation;
	std::optional<MDXAnimVector> rotation;
	std::optional<MDXAnimVector> scale;
};

// -----------------------------------------------------------------------
// MDX ribbon emitter
// -----------------------------------------------------------------------
struct MDXRibbonEmitter : MDXNode {
	float heightAbove = 0.0f;
	float heightBelow = 0.0f;
	float alpha = 0.0f;
	std::vector<float> color;
	float lifeSpan = 0.0f;
	int32_t textureSlot = 0;
	int32_t edgesPerSec = 0;
	int32_t rows = 0;
	int32_t columns = 0;
	int32_t materialId = 0;
	float gravity = 0.0f;
	float visibility = 1.0f;
	std::optional<MDXAnimVector> visibilityAnim;
	std::optional<MDXAnimVector> heightAboveAnim;
	std::optional<MDXAnimVector> heightBelowAnim;
	std::optional<MDXAnimVector> alphaAnim;
	std::optional<MDXAnimVector> textureSlotAnim;
	std::optional<MDXAnimVector> colorAnim;
};

// -----------------------------------------------------------------------
// MDX loader class
// -----------------------------------------------------------------------
class MDXLoader {
public:
	explicit MDXLoader(BufferWrapper& data);

	void load();

	bool isLoaded;

	uint32_t version;
	MDXModelInfo info;
	MDXCollision collision;
	std::vector<MDXSequence> sequences;
	std::vector<uint32_t> globalSequences;
	std::vector<MDXTexture> textures;
	std::vector<MDXMaterial> materials;
	std::vector<MDXTextureAnim> textureAnims;
	std::vector<MDXGeoset> geosets;
	std::vector<MDXGeosetAnim> geosetAnims;
	std::vector<MDXBone> bones;
	std::vector<MDXNode> helpers;
	std::vector<MDXAttachment> attachments;
	std::vector<MDXEventObject> eventObjects;
	std::vector<MDXParticleEmitter> particleEmitters;
	std::vector<MDXParticleEmitter2> particleEmitters2;
	std::vector<MDXCamera> cameras;
	std::vector<MDXLight> lights;
	std::vector<MDXRibbonEmitter> ribbonEmitters;
	std::vector<MDXHitTestShape> hitTestShapes;
	std::vector<std::vector<float>> pivotPoints;
	std::vector<MDXNode*> nodes;

private:
	BufferWrapper& data;

	void _init_model();
	std::string _read_keyword();
	void _expect_keyword(const std::string& expected, const std::string& error_msg);
	std::string _read_string(uint32_t length);
	void _read_extent(MDXExtent& obj);
	MDXAnimVector _read_anim_vector(const std::string& type);
	void _read_node(MDXNode& node);

	// Chunk handlers
	void parse_VERS();
	void parse_MODL();
	void parse_SEQS();
	void parse_GLBS(uint32_t size);
	void parse_MTLS();
	void parse_TEXS(uint32_t size);
	void parse_GEOS();
	void parse_GEOA();
	void parse_BONE();
	void parse_HELP();
	void parse_ATCH();
	void parse_PIVT(uint32_t size);
	void parse_EVTS();
	void parse_HTST();
	void parse_CLID();
	void parse_PREM();
	void parse_PRE2();
	void parse_CAMS();
	void parse_LITE();
	void parse_TXAN();
	void parse_RIBB();

	// Geoset parsing
	void _parse_geosets_v1300();
	void _parse_geosets_v1500();
};
