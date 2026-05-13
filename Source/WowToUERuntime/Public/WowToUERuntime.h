#pragma once

#include "Modules/ModuleManager.h"

class FWowToUERuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
