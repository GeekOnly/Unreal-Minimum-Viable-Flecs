#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WindTypes.h"
#include "WindVolumeComponent.generated.h"

/**
 * Single source of truth for wind motor parameters.
 * Edit these properties directly on the component.
 * The owning AWindMotorActor reads from here.
 */
UCLASS(ClassGroup = "Wind3D", meta = (BlueprintSpawnableComponent))
class WIND3DINTERACTIVE_API UWindVolumeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWindVolumeComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Shape")
	EWindMotorShape Shape = EWindMotorShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Shape")
	EWindEmissionType EmissionType = EWindEmissionType::Directional;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Shape", meta = (ClampMin = "1.0"))
	float Radius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Shape", meta = (ClampMin = "0.0"))
	float Height = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Force", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
	float Strength = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Force", meta = (ClampMin = "0.0"))
	float VortexAngularSpeed = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor")
	bool bEnabled = true;
};
