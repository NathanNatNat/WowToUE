#include "WowCASCInterface.h"
#include "Logging/LogMacros.h"

#include "core.h"
#include "mpq/mpq-install.h"
#include "constants.h"
#include "log.h"
#include "casc/casc-source-local.h"
#include "casc/build-cache.h"
#include "casc/listfile.h"
#include "casc/blp.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCASC, Log, All);

static void WowLibLogCallback(const char* msg)
{
	UE_LOG(LogWowCASC, Log, TEXT("%s"), UTF8_TO_TCHAR(msg));
}

static AppState GWowAppState;

FWowCASCInterface& FWowCASCInterface::Get()
{
	static FWowCASCInterface Instance;
	return Instance;
}

FWowCASCInterface::FWowCASCInterface()
{
}

FWowCASCInterface::~FWowCASCInterface()
{
	Close();
}

void FWowCASCInterface::EnsureGlobalsInitialized()
{
	if (bGlobalsInitialized) return;

	logging::setCallback(WowLibLogCallback);

	UE_LOG(LogWowCASC, Log, TEXT("Initializing WowLib globals..."));

	constants::init();

	core::view = &GWowAppState;
	core::view->config = nlohmann::json::object();
	core::view->config["cascLocale"] = 0x200; // enGB
	core::view->config["listfileURL"] = "https://github.com/wowdev/wow-listfile/releases/latest/download/community-listfile.csv";
	core::view->config["listfileFallbackURL"] = "";
	core::view->config["listfileCacheRefresh"] = 3;
	core::view->config["cdnRegion"] = "eu";
	core::view->config["cdnFallbackHosts"] = "cdn.blizzard.com";
	core::view->config["tactKeysURL"] = "https://raw.githubusercontent.com/wowdev/TACTKeys/master/WoW.txt";
	core::view->config["tactKeysFallbackURL"] = "";
	core::view->config["dbdURL"] = "https://raw.githubusercontent.com/wowdev/WoWDBDefs/refs/heads/master/definitions/%s.dbd";
	core::view->config["dbdFallbackURL"] = "";
	core::view->config["dbdFilenameURL"] = "https://raw.githubusercontent.com/wowdev/WoWDBDefs/refs/heads/master/manifest.json";
	core::view->config["dbdFilenameFallbackURL"] = "";
	core::view->config["cacheExpiry"] = 7;
	core::view->config["enableUnknownFiles"] = true;
	core::view->config["enableM2Skins"] = true;
	core::view->config["enableSharedTextures"] = true;
	core::view->config["enableSharedChildren"] = true;
	core::view->config["overwriteFiles"] = true;
	core::view->config["exportNamedFiles"] = true;
	core::view->selectedCDNRegion = nlohmann::json::object();
	core::view->selectedCDNRegion["tag"] = "eu";
	core::view->selectedCDNRegion["name"] = "Europe";

	// Initialize build cache integrity system (unblocks file reads from cache)
	casc::initBuildCacheSystem();

	UE_LOG(LogWowCASC, Log, TEXT("WowLib globals initialized (defaults). Cache dir: %s"),
		UTF8_TO_TCHAR(constants::CACHE::DIR().string().c_str()));

	bGlobalsInitialized = true;
}

void FWowCASCInterface::ApplySettings(const FWowSettings& S)
{
	EnsureGlobalsInitialized();

	auto ToStd = [](const FString& In) { return std::string(TCHAR_TO_UTF8(*In)); };

	static const TMap<FString, uint32> LocaleFlags = {
		{TEXT("enUS"), 0x2}, {TEXT("koKR"), 0x4}, {TEXT("frFR"), 0x10},
		{TEXT("deDE"), 0x20}, {TEXT("zhCN"), 0x40}, {TEXT("esES"), 0x80},
		{TEXT("zhTW"), 0x100}, {TEXT("enGB"), 0x200}, {TEXT("esMX"), 0x1000},
		{TEXT("ruRU"), 0x2000}, {TEXT("ptBR"), 0x4000}, {TEXT("itIT"), 0x8000},
		{TEXT("ptPT"), 0x10000}
	};

	uint32 Flag = 0x200; // enGB default
	if (const uint32* Found = LocaleFlags.Find(S.Locale))
		Flag = *Found;

	auto& Cfg = core::view->config;
	Cfg["cascLocale"] = Flag;
	Cfg["cdnRegion"] = ToStd(S.CDNRegion);

	if (!S.ListfileURL.IsEmpty()) Cfg["listfileURL"] = ToStd(S.ListfileURL);
	if (!S.ListfileFallbackURL.IsEmpty()) Cfg["listfileFallbackURL"] = ToStd(S.ListfileFallbackURL);
	Cfg["listfileCacheRefresh"] = S.ListfileRefreshDays;

	if (!S.TactKeysURL.IsEmpty()) Cfg["tactKeysURL"] = ToStd(S.TactKeysURL);
	if (!S.TactKeysFallbackURL.IsEmpty()) Cfg["tactKeysFallbackURL"] = ToStd(S.TactKeysFallbackURL);

	if (!S.CDNFallbackHosts.IsEmpty()) Cfg["cdnFallbackHosts"] = ToStd(S.CDNFallbackHosts);

	if (!S.DBDURL.IsEmpty()) Cfg["dbdURL"] = ToStd(S.DBDURL);
	if (!S.DBDFallbackURL.IsEmpty()) Cfg["dbdFallbackURL"] = ToStd(S.DBDFallbackURL);
	if (!S.DBDFilenameURL.IsEmpty()) Cfg["dbdFilenameURL"] = ToStd(S.DBDFilenameURL);
	if (!S.DBDFilenameFallbackURL.IsEmpty()) Cfg["dbdFilenameFallbackURL"] = ToStd(S.DBDFilenameFallbackURL);

	Cfg["cacheExpiry"] = S.CacheExpiryDays;

	static const TMap<FString, FString> RegionNames = {
		{TEXT("eu"), TEXT("Europe")}, {TEXT("us"), TEXT("US")},
		{TEXT("kr"), TEXT("Korea")}, {TEXT("tw"), TEXT("Taiwan")}, {TEXT("cn"), TEXT("China")}
	};

	core::view->selectedCDNRegion = nlohmann::json::object();
	core::view->selectedCDNRegion["tag"] = ToStd(S.CDNRegion);
	FString RegionName = RegionNames.Contains(S.CDNRegion) ? RegionNames[S.CDNRegion] : S.CDNRegion;
	core::view->selectedCDNRegion["name"] = ToStd(RegionName);

	UE_LOG(LogWowCASC, Log, TEXT("Settings applied: region=%s, locale=%s (0x%X)"), *S.CDNRegion, *S.Locale, Flag);
}

void FWowCASCInterface::Close()
{
	if (core::view && core::view->casc)
	{
		delete core::view->casc;
		core::view->casc = nullptr;
	}
	bIsOpen = false;
	bIsLoaded = false;
}

bool FWowCASCInterface::OpenLocalInstall(const FString& Path, TArray<FWowBuildEntry>& OutBuilds, FString& OutError)
{
	EnsureGlobalsInitialized();
	Close();

	UE_LOG(LogWowCASC, Log, TEXT("Opening local CASC install: %s"), *Path);

	try
	{
		std::string StdPath = TCHAR_TO_UTF8(*Path);
		auto* LocalCASC = new casc::CASCLocal(StdPath);
		LocalCASC->init();

		auto Products = LocalCASC->getProductList();
		UE_LOG(LogWowCASC, Log, TEXT("Found %d product(s)"), static_cast<int>(Products.size()));

		for (const auto& P : Products)
		{
			FWowBuildEntry Entry;
			Entry.Index = P.buildIndex;
			Entry.Label = UTF8_TO_TCHAR(P.label.c_str());
			OutBuilds.Add(Entry);

			UE_LOG(LogWowCASC, Log, TEXT("  [%d] %s"), P.buildIndex, UTF8_TO_TCHAR(P.label.c_str()));
		}

		core::view->casc = LocalCASC;
		bIsOpen = true;
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowCASC, Error, TEXT("OpenLocalInstall failed: %s"), UTF8_TO_TCHAR(e.what()));
		OutError = UTF8_TO_TCHAR(e.what());
		return false;
	}
}

bool FWowCASCInterface::LoadBuild(int32 BuildIndex, int32& OutFileCount, FString& OutError)
{
	if (!bIsOpen || !core::view || !core::view->casc)
	{
		OutError = TEXT("CASC not initialized");
		return false;
	}

	UE_LOG(LogWowCASC, Log, TEXT("Loading build index %d..."), BuildIndex);

	try
	{
		auto* LocalCASC = static_cast<casc::CASCLocal*>(core::view->casc);

		logging::timeLog();
		LocalCASC->load(BuildIndex);
		logging::timeEnd("CASC build loaded");

		auto Entries = LocalCASC->getValidRootEntries();
		OutFileCount = static_cast<int32>(Entries.size());
		bIsLoaded = true;

		UE_LOG(LogWowCASC, Log, TEXT("Build loaded: %d files in root table"), OutFileCount);
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowCASC, Error, TEXT("LoadBuild failed: %s"), UTF8_TO_TCHAR(e.what()));
		OutError = UTF8_TO_TCHAR(e.what());
		return false;
	}
}

void FWowCASCInterface::GetFileList(const FString& SearchFilter, const FString& ExtensionFilter, TArray<FWowFileEntry>& OutFiles, int32 MaxResults)
{
	if (!bIsLoaded) return;

	std::string Search = TCHAR_TO_UTF8(*SearchFilter);
	std::string ExtFilter = TCHAR_TO_UTF8(*ExtensionFilter);

	auto AllEntries = casc::listfile::getFilteredEntries(Search);

	OutFiles.Reset();
	OutFiles.Reserve(FMath::Min(MaxResults, static_cast<int32>(AllEntries.size())));

	int32 Count = 0;
	for (const auto& Entry : AllEntries)
	{
		if (Count >= MaxResults) break;

		if (!ExtFilter.empty())
		{
			size_t DotPos = Entry.fileName.rfind('.');
			if (DotPos == std::string::npos) continue;
			std::string Ext = Entry.fileName.substr(DotPos + 1);
			if (Ext != ExtFilter) continue;
		}

		FWowFileEntry FileEntry;
		FileEntry.FileDataID = Entry.fileDataID;
		FileEntry.FileName = UTF8_TO_TCHAR(Entry.fileName.c_str());
		OutFiles.Add(MoveTemp(FileEntry));
		Count++;
	}
}

bool FWowCASCInterface::DecodeBLP(uint32 FileDataID, FDecodedTexture& OutTexture, FString& OutError)
{
	if (!bIsLoaded || !core::view || !core::view->casc)
	{
		OutError = TEXT("CASC not loaded");
		return false;
	}

	try
	{
		auto* LocalCASC = static_cast<casc::CASCLocal*>(core::view->casc);
		auto BLTEData = LocalCASC->getFileAsBLTE(FileDataID);

		// Force full BLTE decompression before passing to BLPImage
		// (BLPImage takes BufferWrapper by value which would slice off BLTEReader's virtual _checkBounds)
		BLTEData.processAllBlocks();

		casc::BLPImage Blp(std::move(static_cast<BufferWrapper&>(BLTEData)));

		std::vector<uint8_t> Pixels = Blp.toUInt8Array(0, 0b1111);

		OutTexture.Width = Blp.width;
		OutTexture.Height = Blp.height;
		OutTexture.Encoding = Blp.encoding;
		OutTexture.RGBA.SetNumUninitialized(Pixels.size());
		FMemory::Memcpy(OutTexture.RGBA.GetData(), Pixels.data(), Pixels.size());

		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogWowCASC, Error, TEXT("DecodeBLP(%u) failed: %s"), FileDataID, UTF8_TO_TCHAR(e.what()));
		OutError = UTF8_TO_TCHAR(e.what());
		return false;
	}
}

BufferWrapper FWowCASCInterface::GetFileData(uint32 FileDataID)
{
	if (!bIsLoaded || !core::view || !core::view->casc)
		throw std::runtime_error("CASC not loaded");

	auto* LocalCASC = static_cast<casc::CASCLocal*>(core::view->casc);
	auto BLTEData = LocalCASC->getFileAsBLTE(FileDataID);
	BLTEData.processAllBlocks();
	return std::move(static_cast<BufferWrapper&>(BLTEData));
}
