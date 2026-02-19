using UnrealBuildTool;

public class Wind3DInteractiveCPU : ModuleRules
{
	public Wind3DInteractiveCPU(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Wind3DInteractive"     // IWindSolver, WindComponents, WindTypes
		});

		PublicIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractiveCPU/Public"
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractiveCPU/Private"
		});
	}
}
