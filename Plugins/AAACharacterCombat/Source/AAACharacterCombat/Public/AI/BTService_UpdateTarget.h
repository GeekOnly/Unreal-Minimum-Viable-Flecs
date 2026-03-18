#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_UpdateTarget.generated.h"

/**
 * BehaviorTree service: updates blackboard keys with combat-relevant data.
 * Runs at TickInterval (default 0.2s).
 * Updates: TargetActor, DistanceToTarget, bCanAttack, bIsPreparingAttack.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UBTService_UpdateTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateTarget();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

	/** Blackboard key for the target actor */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard key for distance to target */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceKey;

	/** Blackboard key: whether this enemy can attack (assigned by subsystem) */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CanAttackKey;

	/** Blackboard key: whether this enemy is preparing to attack */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PreparingAttackKey;
};
