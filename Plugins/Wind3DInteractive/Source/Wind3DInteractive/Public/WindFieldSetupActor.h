#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindFieldSetupActor.generated.h"

class UBoxComponent;

/**
 * Drag-and-drop actor to setup the Wind Grid volume and visualize it.
 */
UCLASS(ClassGroup = "Wind3D", meta = (DisplayName = "Wind Field Setup"))
class WIND3DINTERACTIVE_API AWindFieldSetupActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AWindFieldSetupActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "1"))
	int32 GridSizeX = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "1"))
	int32 GridSizeY = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "1"))
	int32 GridSizeZ = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "10.0"))
	float CellSize = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid")
	FVector AmbientWind = FVector(100.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bAutoStartDebug = true;

protected:
	virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> PreviewBox;

private:
    void UpdatePreviewBox();
};
