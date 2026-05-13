#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

DECLARE_DELEGATE(FOnCASCSessionReady);
DECLARE_DELEGATE_OneParam(FOnCASCSessionError, const FString&);
DECLARE_DELEGATE_OneParam(FOnCASCSessionProgress, const FString&);

struct FCASCBuildEntry
{
	int32 Index;
	FString Product;
	FString Version;
	FString DisplayName;
};

class FCASCSession
{
public:
	FCASCSession();
	~FCASCSession();

	void Initialize(const FString& WowInstallPath);
	void LoadBuild(int32 BuildIndex);

	bool IsInitialized() const { return bInitialized; }
	bool IsLoaded() const { return bLoaded; }
	const TArray<FCASCBuildEntry>& GetBuilds() const { return Builds; }
	int32 GetFileCount() const { return FileCount; }

	FOnCASCSessionReady OnInitialized;
	FOnCASCSessionReady OnBuildLoaded;
	FOnCASCSessionError OnError;
	FOnCASCSessionProgress OnProgress;

private:
	void InitializeWowLibGlobals();

	bool bInitialized = false;
	bool bLoaded = false;
	TArray<FCASCBuildEntry> Builds;
	int32 FileCount = 0;

	bool bGlobalsInitialized = false;
};
