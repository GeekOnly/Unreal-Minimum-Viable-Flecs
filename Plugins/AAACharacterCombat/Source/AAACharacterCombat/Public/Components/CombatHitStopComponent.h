#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatHitStopComponent.generated.h"

/**
 * Provides AAA-quality hit stop (frame freeze) on combat impact.
 * Briefly pauses the owning character's animation and/or global time
 * to sell the weight of each hit.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AAACHARACTERCOMBAT_API UCombatHitStopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatHitStopComponent();

	/** Pause the owning actor's animation by lowering CustomTimeDilation. */
	UFUNCTION(BlueprintCallable, Category = "Combat|HitStop")
	void ApplyHitStop(float Duration = -1.f, float TimeDilation = -1.f);

	/** Briefly lower the global time dilation for all actors. */
	UFUNCTION(BlueprintCallable, Category = "Combat|HitStop")
	void ApplyGlobalHitStop(float Duration = -1.f, float TimeDilation = -1.f);

	// --- Defaults (public so abilities can read them) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitStop")
	float DefaultHitStopDuration = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitStop")
	float DefaultTimeDilation = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitStop")
	float GlobalHitStopDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitStop")
	float GlobalTimeDilation = 0.1f;

	/** Master switch – disable to skip all hit stop logic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitStop")
	bool bEnableHitStop = true;

private:
	void RestoreLocalTimeDilation();
	void RestoreGlobalTimeDilation();

	FTimerHandle LocalHitStopTimerHandle;
	FTimerHandle GlobalHitStopTimerHandle;

	bool bLocalHitStopActive = false;
	bool bGlobalHitStopActive = false;
};
