#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CombatTypes.h"
#include "CombatMovementComponent.generated.h"

class UCharacterMovementComponent;
class UMotionWarpingComponent;

/**
 * Handles camera-relative movement, rotation, and integrates with MotionWarping.
 * Reads GAS boost attribute for boost multiplier.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AAACHARACTERCOMBAT_API UCombatMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config", meta = (ClampMin = "0.0"))
	float MovementSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config", meta = (ClampMin = "0.0"))
	float RotationSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config", meta = (ClampMin = "0.0"))
	float RunAcceleration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config", meta = (ClampMin = "1.0"))
	float BoostMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Movement|Config", meta = (ClampMin = "0.0"))
	float BoostDuration;

	// --- State (Read Only) ---

	UPROPERTY(BlueprintReadOnly, Category = "Combat Movement|State")
	bool bIsRunning;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Movement|State")
	bool bIsBoosting;

	// --- Functions ---

	/** Set camera-relative movement input (from player input axis) */
	UFUNCTION(BlueprintCallable, Category = "Combat Movement")
	void SetMovementInput(FVector2D Input);

	/** Enable or disable movement processing */
	UFUNCTION(BlueprintCallable, Category = "Combat Movement")
	void SetMovementEnabled(bool bEnabled);

	/** Smoothly rotate the owner towards a world-space direction */
	UFUNCTION(BlueprintCallable, Category = "Combat Movement")
	void RotateTowards(FVector TargetDirection, float DeltaTime);

	/** Rotate the owner to face the camera forward direction */
	UFUNCTION(BlueprintCallable, Category = "Combat Movement")
	void RotateToCamera();

	/** Toggle running state */
	UFUNCTION(BlueprintCallable, Category = "Combat Movement")
	void SetRunning(bool bRun);

	/** Activate a temporary speed boost */
	UFUNCTION(BlueprintCallable, Category = "Combat Movement")
	void ActivateBoost();

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category = "Combat Movement|Events")
	FOnCombatEvent OnBoostActivated;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComp;

	UPROPERTY()
	TWeakObjectPtr<UMotionWarpingComponent> CachedWarpingComp;

	FVector2D CurrentInput;
	bool bMovementEnabled;
	float CurrentSpeedMultiplier;
	float BoostTimeRemaining;

	/** Convert input to camera-relative world direction */
	FVector GetCameraRelativeDirection() const;
};
