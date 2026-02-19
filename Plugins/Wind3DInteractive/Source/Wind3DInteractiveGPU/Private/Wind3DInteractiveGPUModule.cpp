#include "Wind3DInteractiveGPUModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FWind3DInteractiveGPUModule"

void FWind3DInteractiveGPUModule::StartupModule()
{
	// Register shader directory so UE can find .usf files
	const FString ShaderDir = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Wind3DInteractive/Source/Wind3DInteractiveGPU/Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Wind3DInteractiveGPU"), ShaderDir);
}

void FWind3DInteractiveGPUModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWind3DInteractiveGPUModule, Wind3DInteractiveGPU)
