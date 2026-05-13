#pragma once

#include "Modules/ModuleManager.h"

class FWowImporterRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
