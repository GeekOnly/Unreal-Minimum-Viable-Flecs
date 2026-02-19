using UnrealBuildTool;

public class Wind3DInteractiveGPU : ModuleRules
{
	public Wind3DInteractiveGPU(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Wind3DInteractive"     // IWindSolver, WindComponents, WindTypes
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RHI",
			"RenderCore",
			"Renderer",
			"Projects"              // For shader directory mapping
		});

		PublicIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractiveGPU/Public"
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractiveGPU/Private"
		});
	}
}
