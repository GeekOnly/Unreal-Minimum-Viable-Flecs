#include "GAS/Abilities/GA_Counter.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "Data/CombatAttackData.h"
#include "Components/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"

UGA_Counter::UGA_Counter()
{
	AbilityTags.AddTag(CombatGameplayTags::Ability_Counter);
	ActivationOwnedTags.AddTag(CombatGameplayTags::State_Combat_Countering);

	// Block if already in combat action
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Attacking);
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Countering);

	// Default fallback
	FallbackMeleeAbilityTag = CombatGameplayTags::Ability_Melee_Light;
}

void UGA_Counter::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = GetCombatCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Find the closest attacking enemy
	UTargetingComponent* TargetingComp = Character->FindComponentByClass<UTargetingComponent>();
	AActor* AttackingEnemy = nullptr;
	if (TargetingComp)
	{
		AttackingEnemy = TargetingComp->GetClosestAttackingEnemy();
	}

	// If no enemy is actively attacking, cancel
	if (!AttackingEnemy)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Check distance to determine close vs far counter
	const float Distance = FVector::Dist(Character->GetActorLocation(), AttackingEnemy->GetActorLocation());

	if (Distance <= CloseRangeDistance)
	{
		ExecuteCloseCounter(AttackingEnemy);
	}
	else
	{
		RedirectToMelee();
	}
}

void UGA_Counter::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	CounterTarget.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Counter::ExecuteCloseCounter(AActor* Target)
{
	CounterTarget = Target;

	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	// Rotate to face target
	const FVector Direction = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal();
	Character->SetActorRotation(Direction.Rotation());

	// Play dodge montage first
	UAnimMontage* Dodge = DodgeMontage.LoadSynchronous();
	if (Dodge)
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(Dodge);

			// Bind end delegate to transition into counter strike
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &UGA_Counter::OnDodgeMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, Dodge);
			return;
		}
	}

	// If no dodge montage, go directly to counter attack
	OnDodgeMontageEnded(nullptr, false);
}

void UGA_Counter::RedirectToMelee()
{
	// Activate melee ability instead via the ASC
	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (ASC && FallbackMeleeAbilityTag.IsValid())
	{
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(FallbackMeleeAbilityTag);
		ASC->TryActivateAbilitiesByTag(TagContainer);
	}

	// End this ability since we're redirecting
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Counter::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted || !CounterTarget.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
		return;
	}

	AActor* Target = CounterTarget.Get();

	// Load and play counter attack
	UCombatAttackData* AttackData = CounterAttackData.LoadSynchronous();
	if (!AttackData)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Set motion warp towards target for the counter strike
	PlayAttackMontage(AttackData, Target);

	// Apply damage
	ApplyDamageToTarget(Target, AttackData);

	// Apply knockback
	if (AttackData->KnockbackForce > 0.0f)
	{
		const ACharacter* Character = GetCombatCharacter();
		if (Character)
		{
			const FVector KnockbackDir = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal();
			ApplyKnockback(Target, AttackData->KnockbackForce, KnockbackDir);
		}
	}

	// Send counter event through GAS
	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (ASC)
	{
		FGameplayEventData EventData;
		EventData.Instigator = GetCombatCharacter();
		EventData.Target = Target;
		ASC->HandleGameplayEvent(CombatGameplayTags::Event_Combat_Counter, &EventData);
	}
}
