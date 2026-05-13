/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <any>
#include <variant>
#include <memory>
#include <mutex>
#include <optional>
#include <chrono>

#include <nlohmann/json.hpp>

#include "file-writer.h"

namespace casc {
	class CASC;
	class BLPImage;
	namespace locale_flags {
		struct LocaleEntry;
	}
}
namespace mpq {
	class MPQInstall;
}

class EventEmitter {
public:
	using Callback = std::function<void()>;
	using ArgCallback = std::function<void(const std::any&)>;

	void setMaxListeners(int max);
	size_t on(const std::string& event, Callback callback);
	size_t on(const std::string& event, ArgCallback callback);
	size_t once(const std::string& event, Callback callback);
	size_t once(const std::string& event, ArgCallback callback);
	void off(const std::string& event, size_t id);
	void emit(const std::string& event);
	void emit(const std::string& event, const std::any& arg);
	void removeAllListeners(const std::string& event);

private:
	struct Listener {
		size_t id;
		std::variant<Callback, ArgCallback> callback;
	};

	size_t addListener(const std::string& event, std::variant<Callback, ArgCallback> cb);
	void emitImpl(const std::string& event, const std::any* arg);

	int maxListeners = 666;
	size_t nextId = 0;
	std::unordered_map<std::string, std::vector<Listener>> listeners;
};

struct ToastAction {
	std::string label;
	std::function<void()> callback;
};

struct Toast {
	std::string type;
	std::string message;
	std::vector<ToastAction> actions;
	bool closable = true;
};

struct MenuOption {
	std::string label;
	std::string value;
};

struct MenuOptionInt {
	std::string label;
	int value;
};

struct ContextMenus {
	nlohmann::json nodeTextureRibbon;  // Context menu node for the texture ribbon.
	nlohmann::json nodeItem;           // Context menu node for the items listfile.
	nlohmann::json nodeDataTable;      // Context menu node for the data table.
	nlohmann::json nodeListbox;        // Context menu node for generic listbox.
	nlohmann::json nodeMap;            // Context menu node for maps listbox.
	nlohmann::json nodeZone;           // Context menu node for zones listbox.
	bool stateNavExtra = false;        // State controller for the extra nav menu.
	bool stateModelExport = false;     // State controller for the model export menu.
	bool stateCDNRegion = false;       // State controller for the CDN region selection menu.

	void resetAll() {
		stateNavExtra = false;
		stateModelExport = false;
		stateCDNRegion = false;
		nodeTextureRibbon = nullptr;
		nodeItem = nullptr;
		nodeDataTable = nullptr;
		nodeListbox = nullptr;
		nodeMap = nullptr;
		nodeZone = nullptr;
	}
};

struct GuildTabardConfig {
	int background = 0;
	int border_style = 0;
	int border_color = 0;
	int emblem_design = 0;
	int emblem_color = 0;
};

struct ColorPickerPosition {
	int x = 0;
	int y = 0;
};

struct ScrollPosition {
	double scrollRel = 0;
	int scrollIndex = 0;
	int64_t timestamp = 0;
};

struct DropHandler {
	std::vector<std::string> ext;
	std::function<std::string(int)> prompt;
	std::function<void(const std::vector<std::string>&)> process;
};

struct AppState {
	~AppState();
	AppState() = default;
	AppState(AppState&&) = default;
	AppState& operator=(AppState&&) = default;
	int installType = 0;                         // Active install type (MPQ or CASC).
	int isBusy = 0;                              // To prevent race-conditions with multiple tasks.
#ifdef BUILD_RELEASE
	bool isDev = false;
#else
	bool isDev = true;
#endif
	bool isLoading = false;                      // Controls whether the loading overlay is visible.
	std::string loadingProgress;                 // Sets the progress text for the loading screen.
	std::string loadingTitle;                    // Sets the title text for the loading screen.
	double loadPct = -1;                         // Controls active loading bar percentage.
	std::optional<Toast> toast;                  // Controls the currently active toast bar.
	nlohmann::json cdnRegions = nlohmann::json::array(); // CDN region data.
	nlohmann::json selectedCDNRegion;            // Active CDN region.
	bool lockCDNRegion = false;                  // If true, do not programmatically alter the selected CDN region.
	nlohmann::json config = nlohmann::json::object();  // Default/user-set configuration.
	nlohmann::json configEdit = nlohmann::json::object();
	nlohmann::json availableLocalBuilds;         // Local builds to display during source select.
	nlohmann::json availableRemoteBuilds;        // Remote builds to display during source select.
	bool sourceSelectShowBuildSelect = false;    // Controls whether build select is shown.
	casc::CASC* casc = nullptr;                  // Active CASC instance.
	std::unique_ptr<mpq::MPQInstall> mpq;
	int64_t cacheSize = 0;                       // Active size of the user cache.
	std::string userInputTactKey;                // Value of manual tact key field.
	std::string userInputTactKeyName;            // Value of manual tact key name field.
	std::string userInputFilterTextures;
	std::string userInputFilterSounds;
	std::string userInputFilterVideos;
	std::string userInputFilterText;
	std::string userInputFilterFonts;
	std::string userInputFilterModels;
	std::string userInputFilterMaps;
	std::string userInputFilterZones;
	std::string userInputFilterItems;
	std::string userInputFilterItemSets;
	std::string userInputFilterDB2s;
	std::string userInputFilterDataTable;
	std::string userInputFilterRaw;
	std::string userInputFilterLegacyModels;
	std::string userInputFilterDecor;
	std::string userInputFilterCreatures;
	nlohmann::json activeModule;                 // Active module component instance.
	std::vector<nlohmann::json> modNavButtons;   // Module-registered navigation buttons.
	std::vector<nlohmann::json> modContextMenuOptions; // Module-registered context menu options.
	std::string userInputFilterInstall;
	std::vector<std::string> modelQuickFilters = {"m2", "m3", "wmo"};
	std::vector<std::string> legacyModelQuickFilters = {"m2", "mdx", "wmo"};
	std::vector<std::string> audioQuickFilters = {"ogg", "mp3", "unk"};
	std::vector<std::string> textQuickFilters = {"lua", "xml", "txt", "sbt", "wtf", "htm", "toc", "xsd", "srt"};
	std::vector<nlohmann::json> selectionTextures;
	std::vector<nlohmann::json> selectionModels;
	std::vector<nlohmann::json> selectionSounds;
	std::vector<nlohmann::json> selectionText;
	std::vector<nlohmann::json> selectionFonts;
	std::vector<nlohmann::json> selectionMaps;
	std::vector<nlohmann::json> selectionZones;
	std::vector<nlohmann::json> selectionItems;
	std::vector<nlohmann::json> selectionItemSets;
	std::vector<nlohmann::json> selectionDB2s;
	std::vector<nlohmann::json> selectionDataTable;
	std::vector<nlohmann::json> selectionRaw;
	std::vector<nlohmann::json> selectionInstall;
	std::vector<nlohmann::json> selectionLegacyModels;
	std::vector<nlohmann::json> selectionDecor;
	std::vector<nlohmann::json> selectionCreatures;
	bool installStringsView = false;
	std::vector<std::string> installStrings;
	std::string installStringsFileName;
	std::vector<nlohmann::json> selectionInstallStrings;
	std::string userInputFilterInstallStrings;
	std::vector<nlohmann::json> listfileTextures;
	std::vector<nlohmann::json> listfileSounds;
	std::vector<nlohmann::json> listfileText;
	std::vector<nlohmann::json> listfileFonts;
	std::vector<nlohmann::json> listfileModels;
	std::vector<nlohmann::json> listfileItems;
	std::vector<nlohmann::json> listfileItemSets;
	std::vector<int> itemViewerTypeMask;
	std::vector<int> itemViewerQualityMask;
	std::vector<nlohmann::json> listfileRaw;
	std::vector<nlohmann::json> listfileInstall;
	std::vector<nlohmann::json> listfileLegacyModels;
	std::vector<nlohmann::json> listfileDecor;
	std::vector<nlohmann::json> listfileCreatures;
	std::vector<nlohmann::json> decorCategoryMask;
	std::vector<nlohmann::json> decorCategoryGroups;
	std::vector<nlohmann::json> dbdManifest;
	std::vector<nlohmann::json> installTags;
	std::vector<nlohmann::json> tableBrowserHeaders;
	std::vector<nlohmann::json> tableBrowserRows;
	nlohmann::json fileDropPrompt;
	std::string textViewerSelectedText;
	std::string fontPreviewPlaceholder;
	std::string fontPreviewText;
	std::string fontPreviewFontFamily;
	double soundPlayerSeek = 0;
	bool soundPlayerState = false;
	std::string soundPlayerTitle = "No File Selected";
	double soundPlayerDuration = 0;
	nlohmann::json modelViewerContext;
	std::string modelViewerActiveType = "none";
	std::vector<nlohmann::json> modelViewerGeosets;
	std::vector<nlohmann::json> modelViewerSkins;
	std::vector<nlohmann::json> modelViewerSkinsSelection;
	std::vector<nlohmann::json> modelViewerAnims;
	nlohmann::json modelViewerAnimSelection;
	bool modelViewerAnimPaused = false;
	int modelViewerAnimFrame = 0;
	int modelViewerAnimFrameCount = 0;
	std::vector<nlohmann::json> modelViewerWMOGroups;
	std::vector<nlohmann::json> modelViewerWMOSets;
	bool modelViewerAutoAdjust = true;
	nlohmann::json legacyModelViewerContext;
	std::string legacyModelViewerActiveType = "none";
	std::vector<nlohmann::json> legacyModelViewerAnims;
	nlohmann::json legacyModelViewerAnimSelection;
	bool legacyModelViewerAnimPaused = false;
	int legacyModelViewerAnimFrame = 0;
	int legacyModelViewerAnimFrameCount = 0;
	bool legacyModelViewerAutoAdjust = true;
	nlohmann::json creatureViewerContext;
	std::string creatureViewerActiveType = "none";
	std::vector<nlohmann::json> creatureViewerGeosets;
	std::vector<nlohmann::json> creatureViewerSkins;
	std::vector<nlohmann::json> creatureViewerSkinsSelection;
	std::vector<nlohmann::json> creatureViewerWMOGroups;
	std::vector<nlohmann::json> creatureViewerWMOSets;
	bool creatureViewerAutoAdjust = true;
	std::vector<nlohmann::json> creatureViewerAnims;
	nlohmann::json creatureViewerAnimSelection;
	bool creatureViewerAnimPaused = false;
	int creatureViewerAnimFrame = 0;
	int creatureViewerAnimFrameCount = 0;
	std::vector<nlohmann::json> creatureViewerEquipment;
	std::vector<nlohmann::json> creatureViewerUVLayers;
	std::string creatureTexturePreviewURL;
	std::string creatureTexturePreviewUVOverlay;
	int creatureTexturePreviewWidth = 256;
	int creatureTexturePreviewHeight = 256;
	std::string creatureTexturePreviewName;
	uint32_t creatureTexturePreviewTexID = 0;
	uint32_t creatureTexturePreviewUVTexID = 0;
	nlohmann::json decorViewerContext;
	std::string decorViewerActiveType = "none";
	std::vector<nlohmann::json> decorViewerGeosets;
	std::vector<nlohmann::json> decorViewerWMOGroups;
	std::vector<nlohmann::json> decorViewerWMOSets;
	bool decorViewerAutoAdjust = true;
	std::vector<nlohmann::json> decorViewerAnims;
	nlohmann::json decorViewerAnimSelection;
	bool decorViewerAnimPaused = false;
	int decorViewerAnimFrame = 0;
	int decorViewerAnimFrameCount = 0;
	std::vector<nlohmann::json> decorViewerUVLayers;
	std::string decorTexturePreviewURL;
	std::string decorTexturePreviewUVOverlay;
	int decorTexturePreviewWidth = 256;
	int decorTexturePreviewHeight = 256;
	std::string decorTexturePreviewName;
	uint32_t decorTexturePreviewTexID = 0;
	uint32_t decorTexturePreviewUVTexID = 0;
	std::vector<nlohmann::json> legacyModelViewerSkins;
	std::vector<nlohmann::json> legacyModelViewerSkinsSelection;
	std::string legacyModelTexturePreviewURL;
	double modelViewerRotationSpeed = 0;
	std::vector<nlohmann::json> textureRibbonStack;
	int textureRibbonSlotCount = 0;
	int textureRibbonPage = 0;
	std::vector<nlohmann::json> textureAtlasOverlayRegions;
	int textureAtlasOverlayWidth = 0;
	int textureAtlasOverlayHeight = 0;
	int modelTexturePreviewWidth = 256;
	int modelTexturePreviewHeight = 256;
	std::string modelTexturePreviewURL;
	std::string modelTexturePreviewName;
	std::string modelTexturePreviewUVOverlay;
	std::vector<nlohmann::json> modelViewerUVLayers;
	uint32_t modelTexturePreviewTexID = 0;
	uint32_t modelTexturePreviewUVTexID = 0;
	int texturePreviewWidth = 256;
	int texturePreviewHeight = 256;
	std::string texturePreviewURL;
	std::string texturePreviewInfo;
	uint32_t texturePreviewTexID = 0;
	std::vector<nlohmann::json> overrideModelList;
	std::string overrideModelName;
	std::vector<nlohmann::json> overrideTextureList;
	std::string overrideTextureName;
	std::string pendingItemSlotFilter;
	std::optional<int> chrPendingEquipSlot;         // Slot ID pending equip from character tab navigation
	std::vector<nlohmann::json> mapViewerMaps;
	std::vector<nlohmann::json> zoneViewerZones;
	std::vector<nlohmann::json> zonePhases;
	nlohmann::json zonePhaseSelection;
	int selectedZoneExpansionFilter = -1;
	uint32_t zoneMapTexID = 0;
	int zoneMapWidth = 0;
	int zoneMapHeight = 0;
	std::vector<uint8_t> zoneMapPixels;
	bool mapViewerHasWorldModel = false;
	bool mapViewerIsWMOMinimap = false;
	nlohmann::json mapViewerGlobalWMO;
	nlohmann::json mapViewerTileLoader;
	nlohmann::json mapViewerSelectedMap;
	nlohmann::json mapViewerSelectedDir;
	nlohmann::json mapViewerChunkMask;
	nlohmann::json mapViewerGridSize;
	std::vector<nlohmann::json> mapViewerSelection;
	bool mapViewerActive = false;
	nlohmann::json mapViewerMapDir;
	nlohmann::json mapViewerMapName;
	nlohmann::json mapViewerMapId;
	int selectedExpansionFilter = -1;
	nlohmann::json chrModelViewerContext;
	std::vector<nlohmann::json> chrModelViewerAnims;
	nlohmann::json chrModelViewerAnimSelection;
	bool chrModelViewerAnimPaused = false;
	int chrModelViewerAnimFrame = 0;
	int chrModelViewerAnimFrameCount = 0;
	std::vector<nlohmann::json> chrCustRaces;
	std::vector<nlohmann::json> chrCustRacesPlayable;
	std::vector<nlohmann::json> chrCustRacesNPC;
	std::vector<nlohmann::json> chrCustRaceSelection;
	std::vector<nlohmann::json> chrCustModels;
	std::vector<nlohmann::json> chrCustModelSelection;
	std::vector<nlohmann::json> chrCustOptions;
	std::vector<nlohmann::json> chrCustOptionSelection;
	std::vector<nlohmann::json> chrCustChoices;
	std::vector<nlohmann::json> chrCustChoiceSelection;
	std::vector<nlohmann::json> chrCustActiveChoices;
	std::vector<nlohmann::json> chrCustGeosets;
	std::string chrCustTab = "models";
	std::string chrCustRightTab = "geosets";
	bool chrModelLoading = false;
	bool chrShowGeosetControl = false;
	std::string chrExportMenu = "export";
	nlohmann::json colorPickerOpenFor;
	ColorPickerPosition colorPickerPosition;
	int chrImportChrModelID = 0;
	int chrImportTargetModelID = 0;
	std::vector<nlohmann::json> chrImportChoices;
	nlohmann::json chrEquippedItems = nlohmann::json::object();
	nlohmann::json chrEquippedItemSkins = nlohmann::json::object();
	GuildTabardConfig chrGuildTabardConfig;
	nlohmann::json chrEquipmentSlotContext;
	std::optional<int> chrItemPickerSlot;
	std::optional<std::string> chrItemPickerFilter;
	bool exportCancelled = false;
	bool isXmas = false;
	std::shared_ptr<casc::BLPImage> chrCustBakedNPCTexture;
	std::string regexTooltip =
		".* - Matches anything\n"
		"(a|b) - Matches either a or b.\n"
		"[a-f] - Matches characters between a-f.\n"
		"[^a-d] - Matches characters that are not between a-d.\n"
		"\\s - Matches whitespace characters.\n"
		"\\d - Matches any digit.\n"
		"a? - Matches zero or one of a.\n"
		"a* - Matches zero or more of a.\n"
		"a+ - Matches one or more of a.\n"
		"a{3} - Matches exactly 3 of a.";
	ContextMenus contextMenus;
	std::vector<MenuOption> menuButtonTextures;
	std::vector<MenuOption> menuButtonMapExport;
	std::vector<MenuOptionInt> menuButtonTextureQuality;
	std::vector<MenuOptionInt> menuButtonHeightmapResolution;
	std::vector<MenuOptionInt> menuButtonHeightmapBitDepth;
	std::vector<MenuOption> menuButtonModels;
	std::vector<MenuOption> menuButtonLegacyModels;
	std::vector<MenuOption> menuButtonDecor;
	std::vector<MenuOption> menuButtonCreatures;
	std::vector<MenuOption> menuButtonCharacterExport;
	std::vector<MenuOption> menuButtonData;
	std::vector<nlohmann::json> helpArticles;
	std::vector<nlohmann::json> helpFilteredArticles;
	nlohmann::json helpSelectedArticle;
	std::string helpSearchQuery;
};

class BusyLock {
public:
	explicit BusyLock(AppState& state);
	~BusyLock();

	BusyLock(const BusyLock&) = delete;
	BusyLock& operator=(const BusyLock&) = delete;
	BusyLock(BusyLock&& other) noexcept;
	BusyLock& operator=(BusyLock&& other) noexcept;

private:
	AppState* state;
};

namespace core {

extern EventEmitter events;
extern AppState* view;

AppState makeNewView();
BusyLock create_busy_lock();
void showLoadingScreen(int segments = 1, const std::string& title = "Loading, please wait...");
void progressLoadingScreen(const std::string& text = "");
void hideLoadingScreen();
void setToast(const std::string& toastType, const std::string& message,
              const std::vector<ToastAction>& actions = {}, int ttl = 10000, bool closable = true);
void hideToast(bool userCancel = false);
void openExportDirectory();
void openInExplorer(const std::string& path);
void registerDropHandler(DropHandler handler);
const DropHandler* getDropHandler(const std::string& file);
FileWriter openLastExportStream();
void saveScrollPosition(const std::string& key, double scrollRel, int scrollIndex);
std::optional<ScrollPosition> getScrollPosition(const std::string& key);
void postToMainThread(std::function<void()> task);
void drainMainThreadQueue();

inline const std::vector<std::string>& cached_json_strings(
	const std::vector<nlohmann::json>& arr,
	std::vector<std::string>& cache,
	size_t& cached_size) {
	if (arr.size() != cached_size) {
		cached_size = arr.size();
		cache.clear();
		cache.reserve(cached_size);
		for (const auto& item : arr)
			cache.push_back(item.get<std::string>());
	}
	return cache;
}

} // namespace core
