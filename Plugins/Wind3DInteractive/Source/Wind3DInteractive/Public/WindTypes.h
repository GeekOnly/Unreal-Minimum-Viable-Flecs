#pragma once

#include "CoreMinimal.h"
#include "WindTypes.generated.h"

UENUM(BlueprintType)
enum class EWindMotorShape : uint8
{
	Sphere   UMETA(DisplayName = "Sphere"),
	Cylinder UMETA(DisplayName = "Cylinder"),
	Cone     UMETA(DisplayName = "Cone")
};

UENUM(BlueprintType)
enum class EWindEmissionType : uint8
{
	Directional UMETA(DisplayName = "Directional"),
	Omni        UMETA(DisplayName = "Omni"),
	Vortex      UMETA(DisplayName = "Vortex"),
	Moving      UMETA(DisplayName = "Moving", ToolTip = "Wind follows object movement direction (sword slash, projectile)")
};
