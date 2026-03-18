#include "GAS/CombatAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UCombatAbilitySystemComponent::UCombatAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UCombatAbilitySystemComponent::GrantDefaultAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities)
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Abilities)
	{
		if (!AbilityClass)
			continue;

		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, GetOwner());
		GiveAbility(Spec);
	}
}

FActiveGameplayEffectHandle UCombatAbilitySystemComponent::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!EffectClass)
		return FActiveGameplayEffectHandle();

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetOwner());
	FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, Level, Context);

	if (Spec.IsValid())
	{
		return ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UCombatAbilitySystemComponent::ApplyEffectToTarget(AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!Target || !EffectClass)
		return FActiveGameplayEffectHandle();

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
		return FActiveGameplayEffectHandle();

	FGameplayEffectContextHandle Context = MakeEffectContext();
	Context.AddSourceObject(GetOwner());
	FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, Level, Context);

	if (Spec.IsValid())
	{
		return ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
	return FActiveGameplayEffectHandle();
}

bool UCombatAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(AbilityTag);
	return TryActivateAbilitiesByTag(TagContainer);
}

bool UCombatAbilitySystemComponent::IsAbilityActiveByTag(FGameplayTag AbilityTag) const
{
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(AbilityTag);

	TArray<FGameplayAbilitySpec*> Specs;
	const_cast<UCombatAbilitySystemComponent*>(this)->GetActivatableGameplayAbilitySpecsByAllMatchingTags(TagContainer, Specs);

	for (const FGameplayAbilitySpec* Spec : Specs)
	{
		if (Spec->IsActive())
			return true;
	}
	return false;
}

void UCombatAbilitySystemComponent::SendCombatEvent(FGameplayTag EventTag, const FGameplayEventData& Payload)
{
	HandleGameplayEvent(EventTag, &Payload);
}
