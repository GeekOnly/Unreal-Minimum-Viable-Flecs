#pragma once

#include "CoreMinimal.h"
#include "GAS/CombatAbilityBase.h"
#include "GA_WeaponThrow.generated.h"

class UTargetingComponent;

/**
 * God of War-style axe throw and recall.
 * Same ability class handles both modes based on weapon state tags.
 * Throw: detaches weapon, applies physics impulse from camera forward.
 * Recall: bezier curve interpolation back to the hand socket.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UGA_WeaponThrow : public UCombatAbilityBase
{
	GENERATED_BODY()

public:
	UGA_WeaponThrow();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// --- Configuration ---

	/** The weapon actor spawned in the world */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|WeaponThrow")
	TWeakObjectPtr<AActor> WeaponActor;

	/** Socket name on the character mesh to attach weapon to */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|WeaponThrow")
	FName HandSocketName = "weapon_r";

	/** Impulse force for throw */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|WeaponThrow", meta = (ClampMin = "0.0"))
	float ThrowPower = 3000.0f;

	/** Speed of weapon returning during recall */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|WeaponThrow", meta = (ClampMin = "0.0"))
	float RecallSpeed = 2500.0f;

	/** Vertical offset for the bezier curve control point */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|WeaponThrow")
	float CurvePointOffset = 300.0f;

	/** Throw montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|WeaponThrow")
	TSoftObjectPtr<UAnimMontage> ThrowMontage;

	/** Recall (catch) montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|WeaponThrow")
	TSoftObjectPtr<UAnimMontage> RecallMontage;

	/** Distance threshold to consider weapon "caught" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|WeaponThrow", meta = (ClampMin = "1.0"))
	float CatchDistance = 50.0f;

private:
	/** Determine if we should throw or recall */
	bool IsWeaponInFlight() const;

	/** Execute the throw sequence */
	void ExecuteThrow();

	/** Execute the recall sequence */
	void ExecuteRecall();

	/** Tick recall interpolation */
	void TickRecall();

	/** Called when weapon reaches the hand */
	void OnWeaponCaught();

	/** Quadratic bezier interpolation helper */
	static FVector GetQuadraticBezierPoint(const FVector& P0, const FVector& P1, const FVector& P2, float T);

	/** Get socket world transform for the hand */
	FTransform GetHandSocketTransform() const;

	/** Recall state */
	FVector RecallStartLocation;
	float RecallAlpha = 0.0f;
	FTimerHandle RecallTickHandle;
};
