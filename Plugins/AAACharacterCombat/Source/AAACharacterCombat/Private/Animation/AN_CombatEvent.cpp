#include "Animation/AN_CombatEvent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAN_CombatEvent::UAN_CombatEvent()
	: bSendToTarget(false)
{
}

FString UAN_CombatEvent::GetNotifyName_Implementation() const
{
	if (EventTag.IsValid())
	{
		return FString::Printf(TEXT("Combat Event [%s]"), *EventTag.ToString());
	}
	return TEXT("Combat Event [None]");
}

void UAN_CombatEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !EventTag.IsValid())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (!ASC)
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.Instigator = OwnerActor;

	if (bSendToTarget)
	{
		// If sending to target, put the owner in instigator and leave target empty
		// The receiving ability can fill in target as needed
		EventData.EventTag = EventTag;
	}

	ASC->HandleGameplayEvent(EventTag, &EventData);
}
