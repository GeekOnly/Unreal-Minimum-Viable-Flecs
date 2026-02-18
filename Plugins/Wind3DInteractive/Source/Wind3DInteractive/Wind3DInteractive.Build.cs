using UnrealBuildTool;

public class Wind3DInteractive : ModuleRules
{
	public Wind3DInteractive(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FlecsLibrary"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore"
		});

		PublicIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractive/Public"
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			"Wind3DInteractive/Private",
			"Wind3DInteractive/Private/Systems"
		});
	}
}
