#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_CombatAttack.generated.h"

/**
 * BehaviorTree task: make enemy approach and attack the player.
 * Sets AI_State_Approaching tag, moves to player, then activates melee ability.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UBTTask_CombatAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CombatAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	/** Called when MoveToActor completes */
	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result, UBehaviorTreeComponent* OwnerComp);

	/** Range within which the enemy can attack the player */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "50.0"))
	float AttackRange = 200.0f;

	/** Tag of the ability to activate when in range */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FGameplayTag AbilityTag;
};
