#pragma once

#include "CoreMinimal.h"
#include "GAS/CombatAbilityBase.h"
#include "GA_WarpStrike.generated.h"

class UCombatAttackData;
class UTargetingComponent;
class UAbilityTask_PlayMontageAndWait;
class UCameraShakeBase;

/**
 * FFXV-style warp strike: teleport to a target with InExpo easing,
 * then apply damage, knockback, and visual feedback on arrival.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UGA_WarpStrike : public UCombatAbilityBase
{
	GENERATED_BODY()

public:
	UGA_WarpStrike();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Configuration ---

	/** Duration of the warp interpolation in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float WarpDuration = 0.25f;

	/** Exponent for InExpo ease curve (higher = snappier arrival) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float WarpEaseExponent = 3.0f;

	/** Montage played during the warp */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp")
	TSoftObjectPtr<UAnimMontage> WarpMontage;

	/** Attack data for the strike on arrival */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp")
	TSoftObjectPtr<UCombatAttackData> WarpAttackData;

	/** Knockback force applied on hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp", meta = (ClampMin = "0.0"))
	float WarpKnockbackForce = 800.0f;

	/** Camera shake on landing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp")
	TSubclassOf<UCameraShakeBase> LandingCameraShake;

	/** Minimum distance to target required to activate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp", meta = (ClampMin = "0.0"))
	float MinWarpDistance = 200.0f;

	/** Maximum distance to target allowed for activation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp", meta = (ClampMin = "0.0"))
	float MaxWarpDistance = 3000.0f;

	/** Stop offset from the target (avoid clipping) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Warp", meta = (ClampMin = "0.0"))
	float ArrivalOffset = 120.0f;

private:
	/** Begin the warp interpolation towards target */
	void BeginWarp(AActor* Target);

	/** Tick the warp interpolation */
	void TickWarp();

	/** Called when warp arrives at target */
	void OnWarpArrived();

	/** InExpo ease function */
	static float EaseInExpo(float T, float Exponent);

	/** Warp state */
	FVector WarpStartLocation;
	FVector WarpEndLocation;
	float WarpElapsed = 0.0f;
	FTimerHandle WarpTickHandle;

	UPROPERTY()
	TWeakObjectPtr<AActor> WarpTarget;
};
