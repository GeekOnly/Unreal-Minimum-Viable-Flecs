#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
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

	/** Top radius for Cylinder shape. 0 = same as Radius. Different from Radius creates a frustum/cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Shape", meta = (ClampMin = "0.0", EditCondition = "Shape == EWindMotorShape::Cylinder"))
	float TopRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Shape", meta = (ClampMin = "0.0"))
	float Height = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Force", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
	float Strength = 500.f;

	/** Extra injection multiplier. 1.0 = normal. 3.0 = powerful shockwave.
	 *  For Moving type, stacks on top of the automatic speed-based scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Force", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float ImpulseScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Force", meta = (ClampMin = "0.0"))
	float VortexAngularSpeed = 1.f;

	/** Trail length behind the motor for Moving emission type. Extends the wind capsule backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Moving", meta = (ClampMin = "0.0", EditCondition = "EmissionType == EWindEmissionType::Moving"))
	float MoveLength = 500.f;

	/** Angular speed for rotating objects (rad/s). Generates tangential wind from rotation.
	 *  Used by Moving emission — rotating fans/turbines create wind even when position is stationary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Moving", meta = (ClampMin = "0.0", EditCondition = "EmissionType == EWindEmissionType::Moving"))
	float AngularSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor")
	bool bEnabled = true;

	// --- Wave Mode ---

	/** Enable pulsing wave output. Simulates real fan blade beat — wind strength oscillates between WaveMin..1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Wave")
	bool bWaveMode = false;

	/** Pulses per second. Fan with 3 blades at 2 RPS = 6 Hz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Wave",
		meta = (ClampMin = "0.01", ClampMax = "30.0", EditCondition = "bWaveMode"))
	float WaveFrequency = 2.f;

	/** Minimum strength factor at wave trough (0 = full off, 0.2 = gentle lull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Wave",
		meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bWaveMode"))
	float WaveMin = 0.1f;

	/** Phase offset in seconds — stagger multiple motors so waves don't sync up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Wave",
		meta = (EditCondition = "bWaveMode"))
	float WavePhaseOffset = 0.f;

	// --- Lifetime & Curves (GoW-style) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Lifetime")
	bool bUseLifetime = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Lifetime", meta = (EditCondition = "bUseLifetime"))
	bool bLoop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Lifetime", meta = (ClampMin = "0.1", EditCondition = "bUseLifetime"))
	float LifeTime = 5.f;

	/** Multiplier curve over lifetime (0..1 normalized time). Default: flat 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Lifetime", meta = (EditCondition = "bUseLifetime"))
	FRuntimeFloatCurve ForceCurve;

	/** Radius multiplier curve over lifetime (0..1 normalized time). Default: flat 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Motor|Lifetime", meta = (EditCondition = "bUseLifetime"))
	FRuntimeFloatCurve RadiusCurve;
};
