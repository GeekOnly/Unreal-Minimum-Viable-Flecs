#include "GAS/Abilities/GA_MeleeAttack.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "Data/CombatAttackData.h"
#include "Data/CombatComboData.h"
#include "Components/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "TimerManager.h"

UGA_MeleeAttack::UGA_MeleeAttack()
{
	AbilityTags.AddTag(CombatGameplayTags::Ability_Melee_Light);
	ActivationOwnedTags.AddTag(CombatGameplayTags::State_Combat_Attacking);

	// Block activation while already attacking or stunned
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Attacking);
}

void UGA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	// Cache targeting component
	CachedTargetingComp = Character->FindComponentByClass<UTargetingComponent>();

	// Find target
	AActor* Target = nullptr;
	if (CachedTargetingComp.IsValid())
	{
		Target = CachedTargetingComp->GetCurrentTarget();
	}

	// Check combo reset timeout
	const double CurrentTime = FPlatformTime::Seconds();
	const UCombatComboData* Combo = ComboData.LoadSynchronous();
	if (Combo)
	{
		if (CurrentTime - LastAttackTime > Combo->ComboResetTime)
		{
			ResetCombo();
		}
	}

	ExecuteComboAttack(Target);
}

void UGA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const ACharacter* Character = GetCombatCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(ComboResetTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MeleeAttack::ExecuteComboAttack(AActor* Target)
{
	const UCombatComboData* Combo = ComboData.LoadSynchronous();
	if (!Combo || Combo->ComboChain.Num() == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Clamp combo index
	CurrentComboIndex = FMath::Clamp(CurrentComboIndex, 0, Combo->ComboChain.Num() - 1);

	// Check for finisher
	if (Target && ShouldPlayFinisher(Target))
	{
		UCombatAttackData* FinisherData = Combo->FinisherAttack.LoadSynchronous();
		if (FinisherData)
		{
			// Apply time dilation for dramatic effect
			if (UWorld* World = GetWorld())
			{
				World->GetWorldSettings()->SetTimeDilation(Combo->FinisherTimeDilation);

				// Restore time dilation after finisher duration
				FTimerHandle RestoreHandle;
				const float RestoreDuration = Combo->FinisherDuration * Combo->FinisherTimeDilation;
				World->GetTimerManager().SetTimer(RestoreHandle, [World]()
				{
					if (World && World->GetWorldSettings())
					{
						World->GetWorldSettings()->SetTimeDilation(1.0f);
					}
				}, RestoreDuration, false);
			}

			PlayAttackMontage(FinisherData, Target);
			ApplyDamageToTarget(Target, FinisherData);
			LastAttackTime = FPlatformTime::Seconds();
			ResetCombo();
			return;
		}
	}

	// Get current combo node
	const FComboNode& CurrentNode = Combo->ComboChain[CurrentComboIndex];

	// Resolve attack data: if no target, use whiff attack
	UCombatAttackData* AttackData = nullptr;
	if (Target)
	{
		AttackData = CurrentNode.AttackData.LoadSynchronous();
	}
	else
	{
		AttackData = Combo->WhiffAttack.LoadSynchronous();
	}

	if (!AttackData)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Play the attack
	PlayAttackMontage(AttackData, Target);

	if (Target)
	{
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
	}

	// Advance combo index for next activation
	const int32 NextIndex = bIsHeavyAttack ? CurrentNode.NextHeavyIndex : CurrentNode.NextLightIndex;
	if (NextIndex >= 0 && NextIndex < Combo->ComboChain.Num())
	{
		CurrentComboIndex = NextIndex;
	}
	else
	{
		// End of chain, reset
		ResetCombo();
	}

	// Record attack time and set reset timer
	LastAttackTime = FPlatformTime::Seconds();

	if (const ACharacter* Character = GetCombatCharacter())
	{
		Character->GetWorldTimerManager().SetTimer(
			ComboResetTimerHandle,
			FTimerDelegate::CreateUObject(this, &UGA_MeleeAttack::OnComboResetTimerExpired),
			Combo->ComboResetTime,
			false
		);
	}
}

bool UGA_MeleeAttack::ShouldPlayFinisher(AActor* Target) const
{
	if (!CachedTargetingComp.IsValid())
		return false;

	// Finisher: only 1 enemy left and target is at low HP
	if (CachedTargetingComp->GetAliveEnemyCount() > 1)
		return false;

	// Check target HP via GAS attribute
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
		return false;

	// Look for a health attribute - check if target has low HP by seeing if damage tag threshold is met
	// Use a generic check: if the target has the stunned state, treat as finisher-ready
	// In production, query the attribute set directly
	return TargetASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Stunned);
}

void UGA_MeleeAttack::ResetCombo()
{
	CurrentComboIndex = 0;
}

void UGA_MeleeAttack::OnComboResetTimerExpired()
{
	ResetCombo();
}
