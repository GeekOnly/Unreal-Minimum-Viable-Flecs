#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CombatRetreat.generated.h"

/**
 * BehaviorTree task: make enemy retreat away from the player.
 * Sets AI_State_Retreating tag, moves to a point away from the player,
 * then removes the tag on arrival.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UBTTask_CombatRetreat : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CombatRetreat();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result, UBehaviorTreeComponent* OwnerComp);

	/** How far to retreat from the player */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "100.0"))
	float RetreatDistance = 500.0f;
};
