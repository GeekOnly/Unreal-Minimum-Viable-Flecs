using UnrealBuildTool;
using System.IO;

public class Wind3DInteractiveEditor : ModuleRules
{
	public Wind3DInteractiveEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Wind3DInteractive"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"ComponentVisualizers",
			"Slate",
			"SlateCore",
			"AssetRegistry",
			"RenderCore",
			"FlecsLibrary"
		});

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
	}
}
