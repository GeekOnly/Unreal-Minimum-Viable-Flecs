#include "AI/BTService_UpdateTarget.h"
#include "Core/CombatGameplayTags.h"
#include "AI/CombatAIManagerSubsystem.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"

UBTService_UpdateTarget::UBTService_UpdateTarget()
{
	NodeName = "Update Combat Target";
	Interval = 0.2f;
	RandomDeviation = 0.05f;

	// Accept object and bool/float key types
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTarget, TargetActorKey), AActor::StaticClass());
	DistanceKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTarget, DistanceKey));
	CanAttackKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTarget, CanAttackKey));
	PreparingAttackKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTarget, PreparingAttackKey));
}

void UBTService_UpdateTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (BBAsset)
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
		DistanceKey.ResolveSelectedKey(*BBAsset);
		CanAttackKey.ResolveSelectedKey(*BBAsset);
		PreparingAttackKey.ResolveSelectedKey(*BBAsset);
	}
}

void UBTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!BB || !AIController)
		return;

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
		return;

	// Find player as target
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(OwnerPawn, 0);

	// Update TargetActor
	BB->SetValueAsObject(TargetActorKey.SelectedKeyName, PlayerPawn);

	// Update DistanceToTarget
	if (PlayerPawn)
	{
		const float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), PlayerPawn->GetActorLocation());
		BB->SetValueAsFloat(DistanceKey.SelectedKeyName, Distance);
	}

	// Update bCanAttack: check if this enemy has PreparingAttack tag (assigned by subsystem)
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (ASC)
	{
		const bool bIsPreparing = ASC->HasMatchingGameplayTag(CombatGameplayTags::AI_State_PreparingAttack);
		BB->SetValueAsBool(PreparingAttackKey.SelectedKeyName, bIsPreparing);
		BB->SetValueAsBool(CanAttackKey.SelectedKeyName, bIsPreparing);
	}
}

FString UBTService_UpdateTarget::GetStaticDescription() const
{
	return FString::Printf(TEXT("Update Target Keys (Interval: %.2fs)"), Interval);
}
