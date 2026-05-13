#pragma once

#include "Modules/ModuleManager.h"

class FWowLibModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
