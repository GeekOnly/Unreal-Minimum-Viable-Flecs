#pragma once

#include "CoreMinimal.h"
#include "GAS/CombatAbilityBase.h"
#include "GA_Counter.generated.h"

class UCombatAttackData;
class UTargetingComponent;

/**
 * Counter ability activated when an enemy is preparing to attack.
 * If close: plays dodge montage then counter attack.
 * If far: redirects to melee attack instead.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UGA_Counter : public UCombatAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Counter();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Configuration ---

	/** Dodge montage played before counter strike */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Counter")
	TSoftObjectPtr<UAnimMontage> DodgeMontage;

	/** Counter attack data asset */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Counter")
	TSoftObjectPtr<UCombatAttackData> CounterAttackData;

	/** Maximum distance to perform close-range counter (beyond this, redirects to melee) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Counter", meta = (ClampMin = "0.0"))
	float CloseRangeDistance = 400.0f;

	/** Melee ability tag to activate when target is too far for counter */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Counter")
	FGameplayTag FallbackMeleeAbilityTag;

private:
	/** Execute the dodge + counter sequence */
	void ExecuteCloseCounter(AActor* Target);

	/** Redirect to melee attack when too far */
	void RedirectToMelee();

	/** Called when dodge montage finishes, triggers the actual counter strike */
	UFUNCTION()
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Cached target for counter strike after dodge */
	UPROPERTY()
	TWeakObjectPtr<AActor> CounterTarget;
};
