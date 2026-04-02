using UnrealBuildTool;
using System.IO;

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

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
	}
}
