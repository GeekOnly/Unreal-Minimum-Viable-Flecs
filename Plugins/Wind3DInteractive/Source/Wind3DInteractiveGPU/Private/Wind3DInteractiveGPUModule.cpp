#include "Wind3DInteractiveGPUModule.h"
#include "WindGridGPU.h"
#include "IWindSolver.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FWind3DInteractiveGPUModule"

void FWind3DInteractiveGPUModule::StartupModule()
{
	// Register shader directory so UE can find .usf files at /Wind3DInteractiveGPU/*
	const FString ShaderDir = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("Wind3DInteractive/Source/Wind3DInteractiveGPU/Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Wind3DInteractiveGPU"), ShaderDir);

	// Register GPU solver factory — WindSubsystem calls via FWindSolverFactory::CreateGPU
	FWindSolverFactory::CreateGPU.BindLambda([]() -> TUniquePtr<IWindSolver>
	{
		return MakeUnique<FWindGridGPU>();
	});
}

void FWind3DInteractiveGPUModule::ShutdownModule()
{
	FWindSolverFactory::CreateGPU.Unbind();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWind3DInteractiveGPUModule, Wind3DInteractiveGPU)
