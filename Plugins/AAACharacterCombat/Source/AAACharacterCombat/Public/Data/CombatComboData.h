#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatComboData.generated.h"

class UCombatAttackData;

/**
 * Defines a combo chain — an ordered sequence of attacks.
 * The combo system cycles through these in order, resetting on timeout.
 * Supports branching (light chain vs heavy chain).
 */
USTRUCT(BlueprintType)
struct AAACHARACTERCOMBAT_API FComboNode
{
	GENERATED_BODY()

	/** The attack data for this node in the chain */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UCombatAttackData> AttackData;

	/** If input is "light" at this node, advance to this index. -1 = end combo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 NextLightIndex = -1;

	/** If input is "heavy" at this node, branch to this index. -1 = end combo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 NextHeavyIndex = -1;
};

/**
 * Data asset that defines a full combo tree.
 * Create one per character class or weapon type.
 */
UCLASS(BlueprintType)
class AAACHARACTERCOMBAT_API UCombatComboData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText ComboName;

	/** Ordered combo chain with branching support */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	TArray<FComboNode> ComboChain;

	/** Time (seconds) since last attack before combo resets to 0 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	float ComboResetTime = 1.5f;

	/** Attacks used when no target is found (whiff / air swing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	TSoftObjectPtr<UCombatAttackData> WhiffAttack;

	/** Special finisher attack used when this is the last enemy alive with 1 HP */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	TSoftObjectPtr<UCombatAttackData> FinisherAttack;

	/** Time dilation during finisher */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	float FinisherTimeDilation = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo")
	float FinisherDuration = 2.0f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("CombatCombo", GetFName());
	}
};
