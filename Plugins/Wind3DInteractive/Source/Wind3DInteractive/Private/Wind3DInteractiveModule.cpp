#include "Wind3DInteractiveModule.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FWind3DInteractiveModule, Wind3DInteractive);

DEFINE_LOG_CATEGORY(LogWind3D);

void FWind3DInteractiveModule::StartupModule()
{
	UE_LOG(LogWind3D, Log, TEXT("Wind3DInteractive module has started."));
}

void FWind3DInteractiveModule::ShutdownModule()
{
	UE_LOG(LogWind3D, Log, TEXT("Wind3DInteractive module has shut down."));
}
