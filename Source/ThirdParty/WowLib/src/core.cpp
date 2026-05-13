/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cstdlib>
#endif

#include "core.h"
#include "generics.h"
#include "casc/locale-flags.h"
#include "constants.h"
#include "log.h"
#include "mpq/mpq-install.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <memory>
#include <thread>


AppState::~AppState() = default;


void EventEmitter::setMaxListeners(int max) {
	maxListeners = max;
}

size_t EventEmitter::addListener(const std::string& event, std::variant<Callback, ArgCallback> cb) {
	size_t id = nextId++;
	listeners[event].push_back({ id, std::move(cb) });
	return id;
}

size_t EventEmitter::on(const std::string& event, Callback callback) {
	return addListener(event, std::move(callback));
}

size_t EventEmitter::on(const std::string& event, ArgCallback callback) {
	return addListener(event, std::move(callback));
}

size_t EventEmitter::once(const std::string& event, Callback callback) {
	auto idPtr = std::make_shared<size_t>(0);
	*idPtr = on(event, Callback([this, event, idPtr, cb = std::move(callback)]() {
		off(event, *idPtr);
		cb();
	}));
	return *idPtr;
}

size_t EventEmitter::once(const std::string& event, ArgCallback callback) {
	auto idPtr = std::make_shared<size_t>(0);
	*idPtr = on(event, ArgCallback([this, event, idPtr, cb = std::move(callback)](const std::any& arg) {
		off(event, *idPtr);
		cb(arg);
	}));
	return *idPtr;
}

void EventEmitter::off(const std::string& event, size_t id) {
	auto it = listeners.find(event);
	if (it == listeners.end())
		return;

	auto& vec = it->second;
	vec.erase(
		std::remove_if(vec.begin(), vec.end(), [id](const Listener& l) { return l.id == id; }),
		vec.end()
	);
}

void EventEmitter::emitImpl(const std::string& event, const std::any* arg) {
	auto it = listeners.find(event);
	if (it == listeners.end())
		return;

	auto callbacks = it->second;
	for (const auto& listener : callbacks) {
		std::visit([arg](const auto& cb) {
			using T = std::decay_t<decltype(cb)>;
			if constexpr (std::is_same_v<T, Callback>) {
				cb();
			} else {
				cb(arg ? *arg : std::any{});
			}
		}, listener.callback);
	}
}

void EventEmitter::emit(const std::string& event) {
	emitImpl(event, nullptr);
}

void EventEmitter::emit(const std::string& event, const std::any& arg) {
	emitImpl(event, &arg);
}

void EventEmitter::removeAllListeners(const std::string& event) {
	listeners.erase(event);
}


BusyLock::BusyLock(AppState& state)
	: state(&state) {
	this->state->isBusy++;
}

BusyLock::~BusyLock() {
	if (state)
		state->isBusy--;
}

BusyLock::BusyLock(BusyLock&& other) noexcept
	: state(other.state) {
	other.state = nullptr;
}

BusyLock& BusyLock::operator=(BusyLock&& other) noexcept {
	if (this != &other) {
		if (state)
			state->isBusy--;
		state = other.state;
		other.state = nullptr;
	}
	return *this;
}


namespace core {

// core.events is a global event handler used for dispatching
// events from any point in the system, to any other point.
EventEmitter events;

// The `view` object is used as a reference to the data for the main Vue instance.
AppState* view = nullptr;

// Used by setToast() for TTL toast prompts.
static int toastTimer = -1;
static std::chrono::steady_clock::time_point toastExpiry;

// dropHandlers contains handlers for drag/drop support.
// Each item is an object defining .ext, .prompt() and .process().
static std::vector<DropHandler> dropHandlers;

// scrollPositions stores persistent scroll positions for listbox components
// keyed by persistScrollKey (e.g., "models", "textures", etc.)
static std::unordered_map<std::string, ScrollPosition> scrollPositions;

// internal progress state for loading screen api
static std::atomic<int> loading_progress_segments{1};
static std::atomic<int> loading_progress_value{0};

static std::mutex s_mainQueueMutex;
static std::vector<std::function<void()>> s_mainQueue;

AppState makeNewView() {
	AppState state;

	const auto now = std::chrono::system_clock::now();
	const auto time = std::chrono::system_clock::to_time_t(now);
	std::tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &time);
#else
	localtime_r(&time, &tm_buf);
#endif
	state.isXmas = (tm_buf.tm_mon == 11);

	state.menuButtonTextures = {
		{ "Export as PNG", "PNG" },
		{ "Export as WebP", "WEBP" },
		{ "Export as BLP (Raw)", "BLP" },
		{ "Copy to Clipboard", "CLIPBOARD" }
	};

	state.menuButtonMapExport = {
		{ "Export OBJ", "OBJ" },
		{ "Export PNG", "PNG" },
		{ "Export Minimap Tiles", "MINIMAP" },
		{ "Export Raw", "RAW" },
		{ "Export Heightmaps", "HEIGHTMAPS" },
		{ "Export UE Heightmap", "UE_HEIGHTMAP" }
	};

	state.menuButtonTextureQuality = {
		{ "Alpha Maps", -1 },
		{ "None", 0 },
		{ "Minimap (512)", 512 },
		{ "ADT LoD (1k)", -2 },
		{ "Low (1k)", 1024 },
		{ "Medium (4k)", 4096 },
		{ "High (8k)", 8192 },
		{ "Ultra (16k)", 16384 }
	};

	state.menuButtonHeightmapResolution = {
		{ "64x64", 64 },
		{ "128x128", 128 },
		{ "512x512", 512 },
		{ "1024x1024 (1k)", 1024 },
		{ "2048x2048 (2k)", 2048 },
		{ "4096x4096 (4k)", 4096 },
		{ "Custom", -1 }
	};

	state.menuButtonHeightmapBitDepth = {
		{ "8-bit Depth", 8 },
		{ "16-bit Depth", 16 },
		{ "32-bit Depth", 32 }
	};

	state.menuButtonModels = {
		{ "Export OBJ", "OBJ" },
		{ "Export STL", "STL" },
		{ "Export glTF", "GLTF" },
		{ "Export GLB", "GLB" },
		{ "Export FBX", "FBX" },
		{ "Export M2 / WMO (Raw)", "RAW" },
		{ "Export PNG (3D Preview)", "PNG" },
		{ "Copy to Clipboard (3D Preview)", "CLIPBOARD" }
	};

	state.menuButtonLegacyModels = {
		{ "Export OBJ", "OBJ" },
		{ "Export STL", "STL" },
		{ "Export FBX", "FBX" },
		{ "Export Raw", "RAW" },
		{ "Export PNG (3D Preview)", "PNG" },
		{ "Copy to Clipboard (3D Preview)", "CLIPBOARD" }
	};

	state.menuButtonDecor = {
		{ "Export OBJ", "OBJ" },
		{ "Export STL", "STL" },
		{ "Export glTF", "GLTF" },
		{ "Export GLB", "GLB" },
		{ "Export FBX", "FBX" },
		{ "Export M2 / WMO (Raw)", "RAW" },
		{ "Export PNG (3D Preview)", "PNG" },
		{ "Copy to Clipboard (3D Preview)", "CLIPBOARD" }
	};

	state.menuButtonCreatures = {
		{ "Export OBJ", "OBJ" },
		{ "Export STL", "STL" },
		{ "Export glTF", "GLTF" },
		{ "Export GLB", "GLB" },
		{ "Export FBX", "FBX" },
		{ "Export M2 / WMO (Raw)", "RAW" },
		{ "Export PNG (3D Preview)", "PNG" },
		{ "Copy to Clipboard (3D Preview)", "CLIPBOARD" }
	};

	state.menuButtonCharacterExport = {
		{ "Export glTF", "GLTF" },
		{ "Export GLB", "GLB" },
		{ "Export FBX", "FBX" },
		{ "Export OBJ (Posed)", "OBJ" },
		{ "Export STL (Posed)", "STL" },
		{ "Export PNG (3D Preview)", "PNG" },
		{ "Copy to Clipboard (3D Preview)", "CLIPBOARD" }
	};

	state.menuButtonData = {
		{ "Export as CSV", "CSV" },
		{ "Export as SQL", "SQL" },
		{ "Export DB2 (Raw)", "DB2" }
	};

	return state;
}

FileWriter openLastExportStream() {
	const auto& path = constants::LAST_EXPORT();
	std::error_code ec;
	auto status = std::filesystem::status(path, ec);
	if (ec) {
		// ENOENT is expected if the file doesn't exist yet
		if (ec != std::errc::no_such_file_or_directory)
			logging::write("failed to stat last_export: {}", ec.message());
	} else if (std::filesystem::is_directory(status)) {
		logging::write("last_export path is a directory, removing it");
		std::filesystem::remove_all(path, ec);
	}
	return FileWriter(path, "utf8");
}

BusyLock create_busy_lock() {
	return BusyLock(*view);
}

void showLoadingScreen(int segments, const std::string& title) {
	loading_progress_segments = segments;
	loading_progress_value = 0;
	postToMainThread([segments, title]() {
		view->loadPct = 0;
		view->loadingTitle = title;
		view->isLoading = true;
		view->isBusy++;
	});
}

void progressLoadingScreen(const std::string& text) {
	loading_progress_value++;
	double newPct = std::min(
		static_cast<double>(loading_progress_value) / loading_progress_segments, 1.0
	);

	postToMainThread([newPct, progressText = text]() {
		view->loadPct = newPct;
		if (!progressText.empty())
			view->loadingProgress = progressText;
	});
}

void hideLoadingScreen() {
	postToMainThread([]() {
		view->loadPct = -1;
		view->isLoading = false;
		view->isBusy--;
	});
}

/**
 * Hide the currently active toast prompt.
 */
void hideToast(bool userCancel) {
	// Cancel outstanding toast expiry timer.
	toastTimer = -1;

	view->toast = std::nullopt;

	if (userCancel)
		events.emit("toast-cancelled");
}

/**
 * Display a toast message.
 */
void setToast(const std::string& toastType, const std::string& message,
              const std::vector<ToastAction>& actions, int ttl, bool closable) {
	Toast toast;
	toast.type = toastType;
	toast.message = message;
	toast.actions = actions;
	toast.closable = closable;
	view->toast = toast;

	// Remove any outstanding toast timer we may have.
	toastTimer = -1;

	// Create a timer to remove this toast.
	if (ttl > -1) {
		toastTimer = ttl;
		toastExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ttl);
	}
}

void openExportDirectory() {
	const std::string exportDir = view->config.value("exportDirectory", "");
	openInExplorer(exportDir);
}

void openInExplorer(const std::string& path) {
#ifdef _WIN32
	int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
	std::wstring wpath(wlen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
	ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
	std::string escaped;
	escaped.reserve(path.size() + 2);
	escaped.push_back('\'');
	for (char c : path) {
		if (c == '\'')
			escaped.append("'\\''");
		else
			escaped.push_back(c);
	}
	escaped.push_back('\'');
	std::string cmd = "xdg-open " + escaped + " &";
	std::system(cmd.c_str());
#endif
}

void registerDropHandler(DropHandler handler) {
	// Ensure the extensions are all lower-case.
	for (auto& ext : handler.ext)
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	dropHandlers.push_back(std::move(handler));
}

const DropHandler* getDropHandler(const std::string& file) {
	std::string lowerFile = file;
	std::transform(lowerFile.begin(), lowerFile.end(), lowerFile.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	for (const auto& handler : dropHandlers) {
		for (const auto& ext : handler.ext) {
			if (lowerFile.size() >= ext.size() &&
				lowerFile.compare(lowerFile.size() - ext.size(), ext.size(), ext) == 0)
				return &handler;
		}
	}

	return nullptr;
}

/**
 * Save scroll position for a listbox with the given key.
 */
void saveScrollPosition(const std::string& key, double scrollRel, int scrollIndex) {
	if (key.empty())
		return;

	const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();

	scrollPositions[key] = {
		.scrollRel = scrollRel,
		.scrollIndex = scrollIndex,
		.timestamp = now
	};
}

/**
 * Get saved scroll position for a listbox with the given key.
 */
std::optional<ScrollPosition> getScrollPosition(const std::string& key) {
	if (key.empty())
		return std::nullopt;

	auto it = scrollPositions.find(key);
	if (it == scrollPositions.end())
		return std::nullopt;

	return it->second;
}

void postToMainThread(std::function<void()> task) {
	std::lock_guard lock(s_mainQueueMutex);
	s_mainQueue.push_back(std::move(task));
}

void drainMainThreadQueue() {
	std::vector<std::function<void()>> tasks;
	{
		std::lock_guard lock(s_mainQueueMutex);
		tasks.swap(s_mainQueue);
	}
	for (auto& task : tasks)
		task();

	if (toastTimer > -1 && std::chrono::steady_clock::now() >= toastExpiry) {
		hideToast();
	}
}

} // namespace core