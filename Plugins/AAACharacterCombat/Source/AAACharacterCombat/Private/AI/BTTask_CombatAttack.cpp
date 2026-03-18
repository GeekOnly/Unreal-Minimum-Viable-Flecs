#include "AI/BTTask_CombatAttack.h"
#include "Core/CombatGameplayTags.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_CombatAttack::UBTTask_CombatAttack()
{
	NodeName = "Combat Attack";
	bNotifyTaskFinished = true;
	AbilityTag = CombatGameplayTags::Ability_Melee_Light;
}

EBTNodeResult::Type UBTTask_CombatAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
		return EBTNodeResult::Failed;

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
		return EBTNodeResult::Failed;

	// Find the player
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(OwnerPawn, 0);
	if (!PlayerPawn)
		return EBTNodeResult::Failed;

	// Set AI_State_Approaching tag on owner's ASC
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (ASC)
	{
		ASC->AddLooseGameplayTag(CombatGameplayTags::AI_State_Approaching);
	}

	// Move towards the player
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(PlayerPawn);
	MoveRequest.SetAcceptanceRadius(AttackRange * 0.8f);
	MoveRequest.SetUsePathfinding(true);

	FPathFollowingRequestResult MoveResult = AIController->MoveTo(MoveRequest);

	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Already in range, try to attack
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Approaching);
			ASC->AddLooseGameplayTag(CombatGameplayTags::AI_State_Attacking);

			FGameplayTagContainer TagContainer;
			TagContainer.AddTag(AbilityTag);
			ASC->TryActivateAbilitiesByTag(TagContainer);
		}
		return EBTNodeResult::Succeeded;
	}

	if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		// Bind move completion callback
		AIController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(
			this, &UBTTask_CombatAttack::OnMoveCompleted, &OwnerComp);
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

EBTNodeResult::Type UBTTask_CombatAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();

		APawn* OwnerPawn = AIController->GetPawn();
		if (OwnerPawn)
		{
			UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
			if (ASC)
			{
				ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Approaching);
			}
		}
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_CombatAttack::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result, UBehaviorTreeComponent* OwnerComp)
{
	if (!OwnerComp)
		return;

	AAIController* AIController = OwnerComp->GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Unbind delegate
	AIController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Approaching);
	}

	if (Result.IsSuccess())
	{
		// Check distance to player
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(OwnerPawn, 0);
		if (PlayerPawn)
		{
			const float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), PlayerPawn->GetActorLocation());
			if (Distance <= AttackRange && ASC)
			{
				ASC->AddLooseGameplayTag(CombatGameplayTags::AI_State_Attacking);

				FGameplayTagContainer TagContainer;
				TagContainer.AddTag(AbilityTag);
				ASC->TryActivateAbilitiesByTag(TagContainer);

				FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
				return;
			}
		}
	}

	FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
}

void UBTTask_CombatAttack::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	// Clean up delegate binding
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController && AIController->GetPathFollowingComponent())
	{
		AIController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UBTTask_CombatAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("Attack (Range: %.0f, Tag: %s)"), AttackRange, *AbilityTag.ToString());
}
