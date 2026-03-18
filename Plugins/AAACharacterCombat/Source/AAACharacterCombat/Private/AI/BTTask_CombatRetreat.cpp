#include "AI/BTTask_CombatRetreat.h"
#include "Core/CombatGameplayTags.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_CombatRetreat::UBTTask_CombatRetreat()
{
	NodeName = "Combat Retreat";
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_CombatRetreat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
		return EBTNodeResult::Failed;

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
		return EBTNodeResult::Failed;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(OwnerPawn, 0);
	if (!PlayerPawn)
		return EBTNodeResult::Failed;

	// Set retreating tag
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (ASC)
	{
		ASC->AddLooseGameplayTag(CombatGameplayTags::AI_State_Retreating);
	}

	// Calculate retreat location: direction away from player
	const FVector AwayDir = (OwnerPawn->GetActorLocation() - PlayerPawn->GetActorLocation()).GetSafeNormal();
	const FVector RetreatTarget = OwnerPawn->GetActorLocation() + AwayDir * RetreatDistance;

	// Project onto nav mesh
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(OwnerPawn->GetWorld());
	FNavLocation ProjectedLocation;
	FVector FinalLocation = RetreatTarget;

	if (NavSys && NavSys->ProjectPointToNavigation(RetreatTarget, ProjectedLocation))
	{
		FinalLocation = ProjectedLocation.Location;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(FinalLocation);
	MoveRequest.SetAcceptanceRadius(50.0f);
	MoveRequest.SetUsePathfinding(true);

	FPathFollowingRequestResult MoveResult = AIController->MoveTo(MoveRequest);

	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Retreating);
		}
		return EBTNodeResult::Succeeded;
	}

	if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		AIController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(
			this, &UBTTask_CombatRetreat::OnMoveCompleted, &OwnerComp);
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

EBTNodeResult::Type UBTTask_CombatRetreat::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_CombatRetreat::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result, UBehaviorTreeComponent* OwnerComp)
{
	if (!OwnerComp)
		return;

	AAIController* AIController = OwnerComp->GetAIOwner();
	if (AIController && AIController->GetPathFollowingComponent())
	{
		AIController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}

	APawn* OwnerPawn = AIController ? AIController->GetPawn() : nullptr;
	if (OwnerPawn)
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Retreating);
		}
	}

	FinishLatentTask(*OwnerComp, Result.IsSuccess() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}

void UBTTask_CombatRetreat::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController && AIController->GetPathFollowingComponent())
	{
		AIController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UBTTask_CombatRetreat::GetStaticDescription() const
{
	return FString::Printf(TEXT("Retreat (Distance: %.0f)"), RetreatDistance);
}
