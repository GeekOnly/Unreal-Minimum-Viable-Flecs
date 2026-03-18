#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "EnemyCombatCharacter.generated.h"

class UCombatAbilitySystemComponent;
class UMotionWarpingComponent;
class UGameplayAbility;
class UGameplayEffect;
class UAnimMontage;

/**
 * AAA-grade enemy combat character.
 * Integrates GAS and MotionWarping. Auto-registers with the CombatAIManagerSubsystem
 * and the player's TargetingComponent on BeginPlay.
 */
UCLASS()
class AAACHARACTERCOMBAT_API AEnemyCombatCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AEnemyCombatCharacter();

	// --- IAbilitySystemInterface ---
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Get the typed combat ASC */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UCombatAbilitySystemComponent* GetCombatAbilitySystemComponent() const { return AbilitySystemComp; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UCombatAbilitySystemComponent> AbilitySystemComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComp;

	// --- GAS Data ---

	/** Abilities granted at startup */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Effects applied at startup (e.g., base stats) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TArray<TSubclassOf<UGameplayEffect>> DefaultEffects;

	// --- Animation ---

	/** Montage played on hit react */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** Montage played on death */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

private:
	/** Initialize GAS: grant abilities and apply effects */
	void InitializeAbilitySystem();

	/** Register with the AI manager subsystem and the player's targeting component */
	void RegisterWithSystems();

	/** Unregister from systems */
	void UnregisterFromSystems();
};
