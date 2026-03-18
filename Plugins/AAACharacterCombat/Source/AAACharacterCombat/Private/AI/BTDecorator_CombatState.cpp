#include "AI/BTDecorator_CombatState.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UBTDecorator_CombatState::UBTDecorator_CombatState()
{
	NodeName = "Combat State Check";
}

bool UBTDecorator_CombatState::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
		return false;

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
		return false;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (!ASC)
		return false;

	const bool bHasTag = ASC->HasMatchingGameplayTag(TagToCheck);
	return bInvertCondition ? !bHasTag : bHasTag;
}

FString UBTDecorator_CombatState::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s Tag: %s"),
		bInvertCondition ? TEXT("NOT") : TEXT("Has"),
		*TagToCheck.ToString());
}
