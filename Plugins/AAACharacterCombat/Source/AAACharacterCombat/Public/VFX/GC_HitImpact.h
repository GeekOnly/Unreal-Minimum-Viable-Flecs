#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GC_HitImpact.generated.h"

class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

/**
 * Gameplay Cue handler for hit impact VFX / SFX.
 * Spawns a Niagara particle system, plays a sound, and optionally
 * triggers a camera shake at the impact location.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UGC_HitImpact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGC_HitImpact();

	virtual bool HandlesEvent(EGameplayCueEvent EventType) const override;

	virtual void HandleGameplayCue(
		AActor* MyTarget,
		EGameplayCueEvent EventType,
		const FGameplayCueParameters& Parameters) override;

protected:
	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const;

	// --- Properties ---

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitImpact|VFX")
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitImpact|SFX")
	TSoftObjectPtr<USoundBase> ImpactSFX;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitImpact|VFX")
	FVector VFXScale = FVector(1.0, 1.0, 1.0);

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitImpact|VFX")
	bool bAttachToTarget = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitImpact|CameraShake")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitImpact|CameraShake")
	float CameraShakeScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitImpact|CameraShake")
	float CameraShakeRadius = 500.f;
};
