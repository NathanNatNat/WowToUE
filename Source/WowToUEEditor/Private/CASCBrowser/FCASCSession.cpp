#include "CASCBrowser/FCASCSession.h"
#include "WowCASCInterface.h"

FCASCSession::FCASCSession()
{
}

FCASCSession::~FCASCSession()
{
}

void FCASCSession::InitializeWowLibGlobals()
{
}

void FCASCSession::Initialize(const FString& WowInstallPath)
{
	Async(EAsyncExecution::Thread, [this, Path = WowInstallPath]()
	{
		TArray<FWowBuildEntry> WowBuilds;
		FString Error;

		bool bSuccess = FWowCASCInterface::Get().OpenLocalInstall(Path, WowBuilds, Error);

		if (bSuccess)
		{
			TArray<FCASCBuildEntry> NewBuilds;
			for (const auto& B : WowBuilds)
			{
				FCASCBuildEntry Entry;
				Entry.Index = B.Index;
				Entry.Product = B.Label;
				Entry.Version = TEXT("");
				Entry.DisplayName = B.Label;
				NewBuilds.Add(Entry);
			}

			AsyncTask(ENamedThreads::GameThread, [this, NewBuilds = MoveTemp(NewBuilds)]() mutable
			{
				Builds = MoveTemp(NewBuilds);
				bInitialized = true;
				OnInitialized.ExecuteIfBound();
			});
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, Error]()
			{
				OnError.ExecuteIfBound(Error);
			});
		}
	});
}

void FCASCSession::LoadBuild(int32 BuildIndex)
{
	Async(EAsyncExecution::Thread, [this, BuildIndex]()
	{
		int32 Count = 0;
		FString Error;

		bool bSuccess = FWowCASCInterface::Get().LoadBuild(BuildIndex, Count, Error);

		if (bSuccess)
		{
			AsyncTask(ENamedThreads::GameThread, [this, Count]()
			{
				FileCount = Count;
				bLoaded = true;
				OnBuildLoaded.ExecuteIfBound();
			});
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this, Error]()
			{
				OnError.ExecuteIfBound(Error);
			});
		}
	});
}
