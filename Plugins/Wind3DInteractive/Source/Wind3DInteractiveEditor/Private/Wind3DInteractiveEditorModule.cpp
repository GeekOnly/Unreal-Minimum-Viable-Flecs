#include "Wind3DInteractiveEditorModule.h"
#include "Modules/ModuleManager.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "WindMotorVisualizer.h"
#include "WindVolumeComponent.h"

IMPLEMENT_MODULE(FWind3DInteractiveEditorModule, Wind3DInteractiveEditor);

void FWind3DInteractiveEditorModule::StartupModule()
{
	RegisterComponentVisualizers();
}

void FWind3DInteractiveEditorModule::ShutdownModule()
{
	UnregisterComponentVisualizers();
}

void FWind3DInteractiveEditorModule::RegisterComponentVisualizers()
{
	if (GUnrealEd)
	{
		TSharedPtr<FWindMotorVisualizer> Visualizer = MakeShareable(new FWindMotorVisualizer());
		const FName CompName = UWindVolumeComponent::StaticClass()->GetFName();
		GUnrealEd->RegisterComponentVisualizer(CompName, Visualizer);
		RegisteredVisualizerNames.Add(CompName);
	}
}

void FWind3DInteractiveEditorModule::UnregisterComponentVisualizers()
{
	if (GUnrealEd)
	{
		for (const FName& Name : RegisteredVisualizerNames)
		{
			GUnrealEd->UnregisterComponentVisualizer(Name);
		}
	}
	RegisteredVisualizerNames.Empty();
}
