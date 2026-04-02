using UnrealBuildTool;
using System.IO;

public class FlecsLibrary : ModuleRules
{
	public FlecsLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		if (Target.Platform != UnrealTargetPlatform.Win64)
			AppendStringToPublicDefinition("flecs_EXPORTS", "0");
	}
}