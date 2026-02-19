using UnrealBuildTool;

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
			"FlecsLibrary",
			"UnrealEd",
			"ComponentVisualizers",
			"Slate",
			"SlateCore",
			"AssetRegistry",
			"RenderCore"
		});

		PublicIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractiveEditor/Public"
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractiveEditor/Private"
		});
	}
}
