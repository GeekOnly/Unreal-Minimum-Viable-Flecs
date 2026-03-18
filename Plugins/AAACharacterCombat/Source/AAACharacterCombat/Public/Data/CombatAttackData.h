#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Core/CombatTypes.h"
#include "CombatAttackData.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;

/**
 * Data-driven attack definition.
 * Each attack in a combo chain references one of these.
 * Designers configure everything here — no C++ changes needed for new attacks.
 */
UCLASS(BlueprintType)
class AAACHARACTERCOMBAT_API UCombatAttackData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Display name for debugging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	/** Tag that identifies this attack (e.g. Ability.Melee.Light) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FGameplayTag AttackTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EAttackType AttackType = EAttackType::Light;

	// --- Animation ---

	/** Montage to play for this attack */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	/** Section name within the montage (if using composite) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FName MontageSectionName = NAME_None;

	/** Playback rate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float PlayRate = 1.0f;

	// --- Motion Warping ---

	/** Enable motion warping towards target during this attack */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping")
	bool bUseMotionWarping = true;

	/** Motion warping target name (must match the warp target in the montage) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping"))
	FName WarpTargetName = "AttackTarget";

	/** Max warp distance — beyond this, the attack won't lunge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping"))
	float MaxWarpDistance = 500.0f;

	/** Stop offset from target (don't clip into them) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Motion Warping", meta = (EditCondition = "bUseMotionWarping"))
	float WarpStopOffset = 100.0f;

	// --- Damage ---

	/** GameplayEffect class to apply on hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Base damage amount (set via SetByCaller magnitude) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	float BaseDamage = 10.0f;

	/** Knockback force applied to hit target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	float KnockbackForce = 300.0f;

	/** Launch angle for air attacks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	float LaunchAngle = 0.0f;

	// --- Timing ---

	/** Cooldown before next attack can be queued */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing")
	float Cooldown = 0.4f;

	/** Window (in seconds from montage start) where input is buffered for combo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing")
	float ComboWindowStart = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing")
	float ComboWindowEnd = 0.8f;

	// --- VFX / SFX ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Polish")
	TSoftObjectPtr<UNiagaraSystem> HitVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Polish")
	TSoftObjectPtr<USoundBase> HitSFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Polish")
	float CameraShakeScale = 1.0f;

	/** Tags required on the owner to use this attack */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Conditions")
	FGameplayTagContainer RequiredTags;

	/** Tags that block this attack */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Conditions")
	FGameplayTagContainer BlockedTags;

	// --- UPrimaryDataAsset ---
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("CombatAttack", GetFName());
	}
};
