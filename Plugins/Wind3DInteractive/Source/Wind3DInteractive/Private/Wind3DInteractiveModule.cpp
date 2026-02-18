#include "Wind3DInteractiveModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

IMPLEMENT_MODULE(FWind3DInteractiveModule, Wind3DInteractive);

DEFINE_LOG_CATEGORY(LogWind3D);

void FWind3DInteractiveModule::StartupModule()
{
	UE_LOG(LogWind3D, Log, TEXT("Wind3DInteractive module has started."));

	// Register shader directory: /Plugin/Wind3DInteractive/ -> <PluginDir>/Shaders/
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Wind3DInteractive"));
	if (Plugin.IsValid())
	{
		const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/Wind3DInteractive"), ShaderDir);
		UE_LOG(LogWind3D, Log, TEXT("Registered shader directory: %s"), *ShaderDir);
	}
}

void FWind3DInteractiveModule::ShutdownModule()
{
	UE_LOG(LogWind3D, Log, TEXT("Wind3DInteractive module has shut down."));
}
