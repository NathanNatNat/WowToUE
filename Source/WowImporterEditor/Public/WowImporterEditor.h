#pragma once

#include "Modules/ModuleManager.h"

class FWowImporterEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> SpawnCASCBrowserTab(const class FSpawnTabArgs& SpawnTabArgs);

	static const FName CASCBrowserTabId;
};
