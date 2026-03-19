#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WindMaterialBuilder.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

/**
 * Editor utility to programmatically create a wind grass material.
 * Creates the complete material graph in C++, bypassing manual node setup.
 * 
 * Usage:
 *   - Console: Wind3D.CreateMaterial [/Game/Wind/M_WindGrass]
 *   - Blueprint: UWindMaterialBuilder::CreateWindMaterial(...)
 */
UCLASS()
class WIND3DINTERACTIVEEDITOR_API UWindMaterialBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create a wind grass material at the given asset path.
	 * Builds the entire node graph programmatically with correct SM6 compatibility.
	 * 
	 * @param SavePath Content path, e.g. "/Game/Wind/M_WindGrass"
	 * @return The created UMaterial, or nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wind3D|Editor", meta = (DevelopmentOnly))
	static UMaterial* CreateWindMaterial(const FString& SavePath = TEXT("/Game/Wind/M_WindGrass_Auto"));

	/**
	 * Create a debugging material that visualizes wind velocity in the volume.
	 * 
	 * @param SavePath Content path, e.g. "/Game/Wind/M_WindDebug"
	 * @return The created UMaterial, or nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wind3D|Editor", meta = (DevelopmentOnly))
	static UMaterial* CreateDebugMaterial(const FString& SavePath = TEXT("/Game/Wind/M_WindDebug_Auto"));

	/**
	 * Apply wind material to a component.
	 * Creates a MID, binds the wind volume texture, and applies it.
	 * 
	 * @param Component Target mesh component
	 * @param WindMaterial Base wind material (from CreateWindMaterial)
	 * @param MaterialIndex Which material slot to replace
	 * @return The created MID, or nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wind3D|Editor", meta = (DevelopmentOnly))
	static UMaterialInstanceDynamic* ApplyWindToComponent(
		UPrimitiveComponent* Component,
		UMaterialInterface* WindMaterial,
		int32 MaterialIndex = 0);

	/** Create a neutral (gray) volume texture asset. */
	static class UVolumeTexture* CreateNeutralWindTexture(const FString& PackagePath);
};
