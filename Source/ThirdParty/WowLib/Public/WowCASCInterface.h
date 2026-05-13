// WowCASCInterface — CASC file system access only.
//
// This class handles connection to a WoW CASC archive (local install or CDN) and
// reading raw file data from it. It does NOT interpret file formats beyond BLP textures.
//
// For asset interpretation (M2 models, WMO, ADT, DB2 lookups), see the loaders in
// WowImporterRuntime (e.g. FWowM2Loader). Those call GetFileData() to get raw bytes
// and handle parsing/coordinate transforms themselves.

#pragma once

#include "CoreMinimal.h"
#include "buffer.h"

struct WOWLIB_API FWowBuildEntry
{
	int32 Index;
	FString Label;
};

struct WOWLIB_API FWowFileEntry
{
	uint32 FileDataID;
	FString FileName;
};

class WOWLIB_API FWowCASCInterface
{
public:
	static FWowCASCInterface& Get();

	struct FWowSettings
	{
		FString CDNRegion;
		FString Locale;
		FString ListfileURL;
		FString ListfileFallbackURL;
		int32 ListfileRefreshDays;
		FString TactKeysURL;
		FString TactKeysFallbackURL;
		FString CDNFallbackHosts;
		FString DBDURL;
		FString DBDFallbackURL;
		FString DBDFilenameURL;
		FString DBDFilenameFallbackURL;
		int32 CacheExpiryDays;
	};

	void ApplySettings(const FWowSettings& Settings);
	bool OpenLocalInstall(const FString& Path, TArray<FWowBuildEntry>& OutBuilds, FString& OutError);
	bool LoadBuild(int32 BuildIndex, int32& OutFileCount, FString& OutError);
	void Close();

	bool IsOpen() const { return bIsOpen; }
	bool IsLoaded() const { return bIsLoaded; }

	void GetFileList(const FString& SearchFilter, const FString& ExtensionFilter, TArray<FWowFileEntry>& OutFiles, int32 MaxResults = 5000);

	struct FDecodedTexture
	{
		TArray<uint8> RGBA;
		uint32 Width = 0;
		uint32 Height = 0;
		uint8 Encoding = 0;
	};

	bool DecodeBLP(uint32 FileDataID, FDecodedTexture& OutTexture, FString& OutError);
	BufferWrapper GetFileData(uint32 FileDataID);

private:
	FWowCASCInterface();
	~FWowCASCInterface();

	bool bGlobalsInitialized = false;
	bool bIsOpen = false;
	bool bIsLoaded = false;

	void EnsureGlobalsInitialized();
};
