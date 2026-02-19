#include "Wind3DInteractiveModule.h"
#include "IWindSolver.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

IMPLEMENT_MODULE(FWind3DInteractiveModule, Wind3DInteractive);

DEFINE_LOG_CATEGORY(LogWind3D);

// Define factory delegate statics (CPU/GPU modules bind at startup)
FWindSolverCreateDelegate FWindSolverFactory::CreateCPU;
FWindSolverCreateDelegate FWindSolverFactory::CreateGPU;

void FWind3DInteractiveModule::StartupModule()
{
	UE_LOG(LogWind3D, Log, TEXT("Wind3DInteractive module has started."));

	// Register shader directory: /Plugin/Wind3DInteractive/ -> <PluginDir>/Shaders/
	// Use plugin's base directory relative to the module file location
	const FString PluginBaseDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Wind3DInteractive"));
	const FString ShaderDir = FPaths::Combine(PluginBaseDir, TEXT("Shaders"));

	if (FPaths::DirectoryExists(ShaderDir))
	{
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/Wind3DInteractive"), ShaderDir);
		UE_LOG(LogWind3D, Log, TEXT("Registered shader directory: %s"), *ShaderDir);
	}
	else
	{
		UE_LOG(LogWind3D, Warning, TEXT("Shader directory not found: %s"), *ShaderDir);
	}
}

void FWind3DInteractiveModule::ShutdownModule()
{
	UE_LOG(LogWind3D, Log, TEXT("Wind3DInteractive module has shut down."));
}
