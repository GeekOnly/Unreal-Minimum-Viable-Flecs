#pragma once

#include "CoreMinimal.h"
#include "GAS/CombatAbilityBase.h"
#include "Core/CombatTypes.h"
#include "GA_ProjectileShot.generated.h"

class UTargetingComponent;
class UAbilityTask_WaitInputRelease;

/**
 * Charge-based projectile ability with precision timing.
 * Hold input to charge, release to fire. Charge quality determines outcome:
 * - Full (>= 0.9): perfect shot towards target with boost
 * - Half (~0.5 +/- precision): good shot with slow-motion feedback
 * - Miss: projectile veers off-target
 */
UCLASS()
class AAACHARACTERCOMBAT_API UGA_ProjectileShot : public UCombatAbilityBase
{
	GENERATED_BODY()

public:
	UGA_ProjectileShot();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Configuration ---

	/** Total time to reach full charge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile", meta = (ClampMin = "0.1"))
	float ChargeDuration = 1.2f;

	/** Precision window around 0.5 for half-charge quality */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile", meta = (ClampMin = "0.01", ClampMax = "0.25"))
	float HalfChargePrecision = 0.1f;

	/** Projectile actor class to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
	TSubclassOf<AActor> ProjectileClass;

	/** Time dilation scale applied during half-charge slow-motion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SlowMotionScale = 0.2f;

	/** Duration of slow-motion effect in real seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile", meta = (ClampMin = "0.1"))
	float SlowMotionDuration = 0.3f;

	/** Charge montage (looping, blended by charge amount) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
	TSoftObjectPtr<UAnimMontage> ChargeMontage;

	/** Fire montage played on release */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
	TSoftObjectPtr<UAnimMontage> FireMontage;

	/** Socket to spawn projectile from */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile")
	FName MuzzleSocketName = "Muzzle";

	/** Speed at which the projectile travels */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 5000.0f;

	/** Angular deviation in degrees for a missed shot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Projectile", meta = (ClampMin = "1.0"))
	float MissDeviationDegrees = 15.0f;

private:
	/** Begin charging */
	void StartCharge();

	/** Tick charge accumulation */
	void TickCharge();

	/** Called when input is released */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Evaluate charge quality and fire */
	void ReleaseShot();

	/** Determine charge quality from normalized charge amount */
	EChargeQuality EvaluateChargeQuality(float NormalizedCharge) const;

	/** Spawn projectile towards direction */
	AActor* SpawnProjectile(const FVector& Origin, const FVector& Direction);

	/** Apply slow-motion effect then restore */
	void ApplySlowMotion();
	void RestoreTimeDilation();

	/** Charge state */
	float CurrentCharge = 0.0f;
	double ChargeStartTime = 0.0;
	FTimerHandle ChargeTickHandle;
	FTimerHandle SlowMotionRestoreHandle;

	/** Cached targeting component */
	UPROPERTY()
	TWeakObjectPtr<UTargetingComponent> CachedTargetingComp;
};
