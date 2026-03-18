#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Core/CombatGameplayTags.h"
#include "CombatAbilityBase.generated.h"

class UCombatAbilitySystemComponent;
class UCombatAttackData;
class UCombatHitStopComponent;
class UCameraShakeBase;

/**
 * Base class for all combat abilities.
 * Provides shared helpers: montage playback, target acquisition,
 * motion warp setup, and damage application via GAS.
 */
UCLASS(Abstract)
class AAACHARACTERCOMBAT_API UCombatAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCombatAbilityBase();

	/** Get the combat ASC from the owning actor */
	UFUNCTION(BlueprintPure, Category = "Combat|Ability")
	UCombatAbilitySystemComponent* GetCombatASC() const;

	/** Get the owning character */
	UFUNCTION(BlueprintPure, Category = "Combat|Ability")
	ACharacter* GetCombatCharacter() const;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Helpers ---

	/** Play a montage and set up motion warp target if applicable */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void PlayAttackMontage(const UCombatAttackData* AttackData, AActor* Target);

	/** Apply damage GE from AttackData to target via GAS */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void ApplyDamageToTarget(AActor* Target, const UCombatAttackData* AttackData);

	/** Apply knockback to target */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void ApplyKnockback(AActor* Target, float Force, FVector Direction);

	/** Set up motion warp target on owner's MotionWarpingComponent */
	void SetMotionWarpTarget(FName WarpName, FVector Location, FRotator Rotation = FRotator::ZeroRotator);

	/** Clear a motion warp target */
	void ClearMotionWarpTarget(FName WarpName);

	/** Apply hit stop to the owning character (frame freeze on impact) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void ApplyHitStop(float Duration = 0.0f);

	/** Play camera shake for local player */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void PlayCombatCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.0f);

	/** Execute full hit response: hit stop + camera shake + GameplayCue for VFX/SFX */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void ExecuteHitResponse(AActor* Target, const UCombatAttackData* AttackData, const FHitResult& HitResult);

	/** Montage ended callback */
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// --- Properties ---

	/** Tags added to owner while this ability is active */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Ability")
	FGameplayTagContainer ActivationOwnedTags;

	/** Tags that cancel this ability if applied to owner */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Ability")
	FGameplayTagContainer CancelAbilityTags;

private:
	FDelegateHandle MontageEndedDelegateHandle;
};
