#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "GameplayTagContainer.h"
#include "BTDecorator_CombatState.generated.h"

/**
 * BehaviorTree decorator: checks if the AI pawn has (or doesn't have) a specific gameplay tag.
 * Uses the pawn's AbilitySystemComponent for tag queries.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UBTDecorator_CombatState : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CombatState();

	virtual FString GetStaticDescription() const override;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	/** The gameplay tag to check on the pawn's ASC */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FGameplayTag TagToCheck;

	/** If true, the condition is inverted (passes when tag is NOT present) */
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bInvertCondition = false;
};
