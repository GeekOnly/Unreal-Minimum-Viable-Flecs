#include "AI/BTTask_StrafeTarget.h"
#include "Core/CombatGameplayTags.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UBTTask_StrafeTarget::UBTTask_StrafeTarget()
{
	NodeName = "Strafe Target";
}

EBTNodeResult::Type UBTTask_StrafeTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	// Set strafing tag
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (ASC)
	{
		ASC->AddLooseGameplayTag(CombatGameplayTags::AI_State_Strafing);
	}

	// Calculate strafe direction: perpendicular to player->enemy vector
	const FVector ToEnemy = (OwnerPawn->GetActorLocation() - PlayerPawn->GetActorLocation()).GetSafeNormal();
	const FVector UpVector = FVector::UpVector;

	// Random left or right
	const float Direction = FMath::RandBool() ? 1.0f : -1.0f;
	const FVector StrafeDir = FVector::CrossProduct(ToEnemy, UpVector).GetSafeNormal() * Direction;

	// Calculate strafe destination
	const float StrafeDistance = StrafeSpeed * StrafeTime;
	const FVector StrafeTarget = OwnerPawn->GetActorLocation() + StrafeDir * StrafeDistance;

	// Project onto nav mesh
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(OwnerPawn->GetWorld());
	FNavLocation ProjectedLocation;
	FVector FinalLocation = StrafeTarget;

	if (NavSys && NavSys->ProjectPointToNavigation(StrafeTarget, ProjectedLocation))
	{
		FinalLocation = ProjectedLocation.Location;
	}

	// Move to strafe point
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(FinalLocation);
	MoveRequest.SetAcceptanceRadius(50.0f);
	MoveRequest.SetUsePathfinding(true);

	AIController->MoveTo(MoveRequest);

	// Set timer to finish strafe after StrafeTime
	UWorld* World = OwnerPawn->GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			StrafeTimerHandle, FTimerDelegate::CreateUObject(
				this, &UBTTask_StrafeTarget::OnStrafeTimeExpired, &OwnerComp),
			StrafeTime, false);
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_StrafeTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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
				ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Strafing);
			}

			UWorld* World = OwnerPawn->GetWorld();
			if (World)
			{
				World->GetTimerManager().ClearTimer(StrafeTimerHandle);
			}
		}
	}

	return EBTNodeResult::Aborted;
}

void UBTTask_StrafeTarget::OnStrafeTimeExpired(UBehaviorTreeComponent* OwnerComp)
{
	if (!OwnerComp)
		return;

	AAIController* AIController = OwnerComp->GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();

		APawn* OwnerPawn = AIController->GetPawn();
		if (OwnerPawn)
		{
			UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
			if (ASC)
			{
				ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Strafing);
			}
		}
	}

	FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}

FString UBTTask_StrafeTarget::GetStaticDescription() const
{
	return FString::Printf(TEXT("Strafe (Speed: %.0f, Time: %.1fs)"), StrafeSpeed, StrafeTime);
}
