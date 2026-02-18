#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FWind3DInteractiveEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterComponentVisualizers();
	void UnregisterComponentVisualizers();

	TArray<FName> RegisteredVisualizerNames;
};
