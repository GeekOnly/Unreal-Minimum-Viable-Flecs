#pragma once

#include "CoreMinimal.h"
#include "GAS/CombatAbilityBase.h"
#include "GA_MeleeAttack.generated.h"

class UCombatComboData;
class UTargetingComponent;
class UAbilityTask_PlayMontageAndWait;

/**
 * Melee combo ability driven by UCombatComboData.
 * Manages combo chain index, light/heavy branching, and finisher detection.
 * AbilityTag defaults to Ability_Melee_Light; override in BP for Heavy variant.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UGA_MeleeAttack : public UCombatAbilityBase
{
	GENERATED_BODY()

public:
	UGA_MeleeAttack();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Configuration ---

	/** Combo data asset defining the attack chain */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee")
	TSoftObjectPtr<UCombatComboData> ComboData;

	/** Whether this activation is a heavy attack (branches to NextHeavyIndex) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee")
	bool bIsHeavyAttack = false;

	/** HP threshold ratio on target to trigger finisher (e.g. 0.15 = 15%) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FinisherHPThreshold = 0.15f;

private:
	/** Resolve the current combo node and execute the attack */
	void ExecuteComboAttack(AActor* Target);

	/** Check whether finisher conditions are met */
	bool ShouldPlayFinisher(AActor* Target) const;

	/** Reset combo state back to root */
	void ResetCombo();

	/** Timer callback for combo reset */
	void OnComboResetTimerExpired();

	/** Current index into the combo chain */
	UPROPERTY()
	int32 CurrentComboIndex = 0;

	/** Timestamp of last successful attack (for combo timeout) */
	double LastAttackTime = 0.0;

	/** Timer handle for combo reset */
	FTimerHandle ComboResetTimerHandle;

	/** Cached targeting component */
	UPROPERTY()
	TWeakObjectPtr<UTargetingComponent> CachedTargetingComp;
};
