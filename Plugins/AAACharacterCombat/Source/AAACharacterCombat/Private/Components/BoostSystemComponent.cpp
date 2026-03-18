#include "Components/BoostSystemComponent.h"
#include "Core/CombatGameplayTags.h"
#include "GAS/CombatAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

UBoostSystemComponent::UBoostSystemComponent()
	: BoostDrainSpeed(20.0f)
	, BoostGainPerHit(10.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBoostSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache the ASC and register for projectile hit events
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC)
	{
		CachedASC = ASC;

		// Listen for Event_Projectile_Hit tag changes via tag count change delegate
		ProjectileHitDelegateHandle = ASC->RegisterGameplayTagEvent(
			CombatGameplayTags::Event_Projectile_Hit,
			EGameplayTagEventType::NewOrRemoved
		).AddUObject(this, &UBoostSystemComponent::OnProjectileHitEvent);
	}
}

void UBoostSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		return;
	}

	// Drain boost while the owner is running
	const bool bRunning = ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Movement_Running);
	if (bRunning && HasBoost())
	{
		const float DrainAmount = BoostDrainSpeed * DeltaTime;
		if (DrainEffect)
		{
			ApplyBoostEffect(DrainEffect, DrainAmount);
		}
	}
}

void UBoostSystemComponent::AddBoost(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	if (GainEffect)
	{
		ApplyBoostEffect(GainEffect, Amount);
		OnBoostVisualActivated.Broadcast();
	}
}

bool UBoostSystemComponent::HasBoost() const
{
	return GetBoostAmount() > 0.0f;
}

float UBoostSystemComponent::GetBoostAmount() const
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		return 0.0f;
	}

	if (const UCombatAttributeSet* Attributes = ASC->GetSet<UCombatAttributeSet>())
	{
		return Attributes->GetBoostAmount();
	}

	return 0.0f;
}

void UBoostSystemComponent::OnProjectileHitEvent(FGameplayTag Tag, int32 NewCount)
{
	// Tag was added (NewCount > 0)
	if (NewCount > 0)
	{
		AddBoost(BoostGainPerHit);
	}
}

UAbilitySystemComponent* UBoostSystemComponent::GetOwnerASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void UBoostSystemComponent::ApplyBoostEffect(TSubclassOf<UGameplayEffect> EffectClass, float Magnitude)
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC || !EffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(GetOwner());

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, ContextHandle);
	if (SpecHandle.IsValid())
	{
		// Set magnitude via SetByCallerTag if the effect uses it, otherwise rely on effect defaults
		SpecHandle.Data->SetSetByCallerMagnitude(CombatGameplayTags::Effect_Boost, Magnitude);
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}
