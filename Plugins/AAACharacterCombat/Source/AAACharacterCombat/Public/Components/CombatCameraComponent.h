#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatCameraComponent.generated.h"

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AAACHARACTERCOMBAT_API UCombatCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatCameraComponent();

	/** Interpolation speed when rotating camera toward lock-on target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat Camera|Lock-On")
	float LockOnInterpSpeed = 8.f;

	/** Pitch offset applied when locked on (negative = look down). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat Camera|Lock-On")
	float LockOnPitchOffset = -15.f;

	/** Field of view while locked on. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat Camera|FOV")
	float LockOnFOV = 75.f;

	/** Default field of view when not locked on. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat Camera|FOV")
	float DefaultFOV = 90.f;

	/** FOV interpolation speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat Camera|FOV")
	float FOVInterpSpeed = 5.f;

	/** Multiplier applied to spring arm length when locked on (< 1 shortens). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat Camera|Lock-On")
	float LockOnArmLengthMultiplier = 0.85f;

	/** Whether the camera is currently locked on to a target. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Camera")
	bool bIsLockedOn = false;

	/** Begin interpolating camera toward the given target. */
	UFUNCTION(BlueprintCallable, Category = "Combat Camera")
	void SetLockOnTarget(AActor* Target);

	/** Return to normal camera behavior. */
	UFUNCTION(BlueprintCallable, Category = "Combat Camera")
	void ClearLockOnTarget();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Perform per-frame camera interpolation toward the lock-on target. */
	void TickCameraUpdate(float DeltaTime);

	TWeakObjectPtr<AActor> LockOnTarget;
};
