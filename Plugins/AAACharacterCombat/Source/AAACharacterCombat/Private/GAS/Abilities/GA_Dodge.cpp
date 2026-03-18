#include "GAS/Abilities/GA_Dodge.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"

UGA_Dodge::UGA_Dodge()
{
	InvincibilityTag = CombatGameplayTags::State_Combat_Invincible;

	// Ability identity
	AbilityTags.AddTag(CombatGameplayTags::Ability_Dodge);

	// Tags added to owner while this ability is active
	ActivationOwnedTags.AddTag(CombatGameplayTags::State_Combat_Dodging);

	// Cannot activate while any of these tags are present
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Dead);
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Stunned);
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Dodging);
}

void UGA_Dodge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = GetCombatCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// --- Grant invincibility via loose tag ---
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(InvincibilityTag);
	}

	// Timer to remove invincibility after the i-frame window
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			InvincibilityTimerHandle,
			this,
			&UGA_Dodge::RemoveInvincibility,
			InvincibilityDuration,
			false
		);
	}

	// --- Determine dodge direction ---
	FVector DodgeDirection = FVector::ZeroVector;

	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		DodgeDirection = MoveComp->GetLastInputVector();
	}

	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = Character->GetActorForwardVector();
	}

	DodgeDirection.Z = 0.f;
	DodgeDirection.Normalize();

	// --- Launch character ---
	Character->LaunchCharacter(DodgeDirection * DodgeSpeed, true, true);

	// --- Play dodge montage ---
	UAnimMontage* Montage = DodgeMontage.LoadSynchronous();
	if (Montage)
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage);

			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &UGA_Dodge::OnDodgeMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
		}
	}
	else
	{
		// No montage – end ability after a short delay matching dodge travel time
		if (UWorld* World = GetWorld())
		{
			FTimerHandle DummyHandle;
			FTimerDelegate EndDelegate;
			EndDelegate.BindLambda([this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			});
			World->GetTimerManager().SetTimer(DummyHandle, EndDelegate, DodgeDistance / FMath::Max(DodgeSpeed, 1.f), false);
		}
	}
}

void UGA_Dodge::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
	}
}

void UGA_Dodge::RemoveInvincibility()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(InvincibilityTag);
	}
}

void UGA_Dodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Clean up invincibility if it's still active
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ASC->HasMatchingGameplayTag(InvincibilityTag))
		{
			ASC->RemoveLooseGameplayTag(InvincibilityTag);
		}
	}

	// Clear the invincibility timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvincibilityTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
