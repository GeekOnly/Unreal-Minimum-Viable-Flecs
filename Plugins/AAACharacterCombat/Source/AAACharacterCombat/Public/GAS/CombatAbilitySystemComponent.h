#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CombatAbilitySystemComponent.generated.h"

/**
 * Project-specific AbilitySystemComponent.
 * Lives on the character (not PlayerState) for simplicity.
 * Provides helper functions for combat-specific operations.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AAACHARACTERCOMBAT_API UCombatAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UCombatAbilitySystemComponent();

	/** Grant a list of abilities at startup */
	void GrantDefaultAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities);

	/** Apply a gameplay effect to self */
	UFUNCTION(BlueprintCallable, Category = "Combat|GAS")
	FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.0f);

	/** Apply a gameplay effect to a target actor's ASC */
	UFUNCTION(BlueprintCallable, Category = "Combat|GAS")
	FActiveGameplayEffectHandle ApplyEffectToTarget(AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.0f);

	/** Try to activate an ability by tag */
	UFUNCTION(BlueprintCallable, Category = "Combat|GAS")
	bool TryActivateAbilityByTag(FGameplayTag AbilityTag);

	/** Check if any ability with this tag is currently active */
	UFUNCTION(BlueprintPure, Category = "Combat|GAS")
	bool IsAbilityActiveByTag(FGameplayTag AbilityTag) const;

	/** Send a gameplay event with optional payload */
	UFUNCTION(BlueprintCallable, Category = "Combat|GAS")
	void SendCombatEvent(FGameplayTag EventTag, const FGameplayEventData& Payload);
};
