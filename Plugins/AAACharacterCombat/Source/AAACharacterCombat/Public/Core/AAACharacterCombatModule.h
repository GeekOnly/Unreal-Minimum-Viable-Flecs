#pragma once

#include "Modules/ModuleManager.h"

class FAAACharacterCombatModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
