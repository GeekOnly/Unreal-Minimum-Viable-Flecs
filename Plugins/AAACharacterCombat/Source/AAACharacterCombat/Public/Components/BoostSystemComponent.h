#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Core/CombatTypes.h"
#include "BoostSystemComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * GAS-integrated boost/stamina system component.
 * Reads/writes BoostAmount attribute through GameplayEffects.
 * Listens for Event_Projectile_Hit on ASC to auto-add boost.
 * Drains boost while the owner is running.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AAACHARACTERCOMBAT_API UBoostSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBoostSystemComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration ---

	/** How fast boost drains per second while running */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Config", meta = (ClampMin = "0.0"))
	float BoostDrainSpeed;

	/** Boost amount gained per projectile hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Config", meta = (ClampMin = "0.0"))
	float BoostGainPerHit;

	/** Gameplay effect applied to drain boost */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Config")
	TSubclassOf<UGameplayEffect> DrainEffect;

	/** Gameplay effect applied to add boost */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Config")
	TSubclassOf<UGameplayEffect> GainEffect;

	// --- Functions ---

	/** Manually add boost amount */
	UFUNCTION(BlueprintCallable, Category = "Boost")
	void AddBoost(float Amount);

	/** Check if boost is above zero */
	UFUNCTION(BlueprintPure, Category = "Boost")
	bool HasBoost() const;

	/** Get current boost amount from the attribute set */
	UFUNCTION(BlueprintPure, Category = "Boost")
	float GetBoostAmount() const;

	// --- Delegates ---

	/** Fired when boost is visually activated (for VFX/UI) */
	UPROPERTY(BlueprintAssignable, Category = "Boost|Events")
	FOnCombatEvent OnBoostVisualActivated;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle ProjectileHitDelegateHandle;

	/** Callback when Event_Projectile_Hit is received */
	void OnProjectileHitEvent(FGameplayTag Tag, int32 NewCount);

	/** Get the owner's ASC */
	UAbilitySystemComponent* GetOwnerASC() const;

	/** Apply a GE to self with a magnitude override */
	void ApplyBoostEffect(TSubclassOf<UGameplayEffect> EffectClass, float Magnitude);
};
