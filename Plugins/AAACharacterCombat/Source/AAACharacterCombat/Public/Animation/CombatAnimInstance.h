#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "CombatAnimInstance.generated.h"

class UAbilitySystemComponent;

/**
 * Custom AnimInstance that exposes combat-specific properties to AnimBP.
 * Reads movement data from CharacterMovementComponent and combat state from GAS tags.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UCombatAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UCombatAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// --- Movement ---

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement")
	float Speed;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement")
	bool bIsInAir;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement")
	bool bIsRunning;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement")
	bool bIsBoosting;

	// --- Combat State ---

	UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
	bool bIsAttacking;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
	bool bIsCountering;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
	bool bIsStunned;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
	bool bIsDead;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
	int32 ComboCount;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
	float ChargeAmount;

	// --- Helpers ---

	/** Returns the owner's AbilitySystemComponent, cached after first lookup */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UAbilitySystemComponent* GetCombatASC() const;

	/** Check whether a specific gameplay tag is currently active on the owner */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsTagActive(FGameplayTag Tag) const;

private:
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	UPROPERTY()
	TWeakObjectPtr<class ACharacter> CachedCharacter;
};
