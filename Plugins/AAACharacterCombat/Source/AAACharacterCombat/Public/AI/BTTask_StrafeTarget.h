#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_StrafeTarget.generated.h"

/**
 * BehaviorTree task: strafe around the player.
 * Picks a random strafe direction (left/right), calculates a point
 * perpendicular to the player-enemy vector, and moves there.
 * Duration-based: returns Succeeded after StrafeTime elapses.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UBTTask_StrafeTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_StrafeTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

private:
	/** Called when strafe duration expires */
	void OnStrafeTimeExpired(UBehaviorTreeComponent* OwnerComp);

	/** Speed at which the enemy strafes */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "50.0"))
	float StrafeSpeed = 200.0f;

	/** How long to strafe before completing */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.5"))
	float StrafeTime = 2.0f;

	FTimerHandle StrafeTimerHandle;
};
