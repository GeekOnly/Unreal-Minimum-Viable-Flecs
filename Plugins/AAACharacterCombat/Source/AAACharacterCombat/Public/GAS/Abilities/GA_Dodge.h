#pragma once

#include "CoreMinimal.h"
#include "GAS/CombatAbilityBase.h"
#include "Core/CombatGameplayTags.h"
#include "GA_Dodge.generated.h"

class UAnimMontage;
class UCharacterMovementComponent;

/**
 * Dodge / roll ability with i-frames.
 * Launches the character in the input direction, plays a dodge montage,
 * and grants brief invincibility via a loose gameplay tag.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UGA_Dodge : public UCombatAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Dodge();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Properties ---

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge")
	TSoftObjectPtr<UAnimMontage> DodgeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge")
	float DodgeDistance = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge")
	float DodgeSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge")
	float InvincibilityDuration = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge")
	float StaminaCost = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge")
	float DodgeCooldownDuration = 0.3f;

private:
	UFUNCTION()
	void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void RemoveInvincibility();

	FTimerHandle InvincibilityTimerHandle;
	FGameplayTag InvincibilityTag;
};
