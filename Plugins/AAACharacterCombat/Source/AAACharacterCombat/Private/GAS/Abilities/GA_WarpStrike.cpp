#include "GAS/Abilities/GA_WarpStrike.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "Data/CombatAttackData.h"
#include "Components/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"

UGA_WarpStrike::UGA_WarpStrike()
{
	AbilityTags.AddTag(CombatGameplayTags::Ability_WarpStrike);
	ActivationOwnedTags.AddTag(CombatGameplayTags::State_Warp_Warping);

	// Block while already warping or in combat
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Warp_Warping);
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Attacking);
}

void UGA_WarpStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	// Find target near screen center
	UTargetingComponent* TargetingComp = Character->FindComponentByClass<UTargetingComponent>();
	AActor* Target = nullptr;
	if (TargetingComp)
	{
		Target = TargetingComp->GetCurrentTarget();
	}

	if (!Target)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Validate distance
	const float Distance = FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation());
	if (Distance < MinWarpDistance || Distance > MaxWarpDistance)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Send warp started event
	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (ASC)
	{
		FGameplayEventData EventData;
		EventData.Instigator = Character;
		EventData.Target = Target;
		ASC->HandleGameplayEvent(CombatGameplayTags::Event_Warp_Started, &EventData);
	}

	BeginWarp(Target);
}

void UGA_WarpStrike::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Clear warp tick timer
	if (const ACharacter* Character = GetCombatCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(WarpTickHandle);
	}

	// Restore movement mode
	ACharacter* Character = GetCombatCharacter();
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	WarpTarget.Reset();
	WarpElapsed = 0.0f;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_WarpStrike::BeginWarp(AActor* Target)
{
	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	WarpTarget = Target;
	WarpStartLocation = Character->GetActorLocation();
	WarpElapsed = 0.0f;

	// Compute arrival point (offset from target)
	const FVector DirectionToTarget = (Target->GetActorLocation() - WarpStartLocation).GetSafeNormal();
	WarpEndLocation = Target->GetActorLocation() - DirectionToTarget * ArrivalOffset;

	// Disable gravity / walking during warp
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Flying);
		Movement->StopMovementImmediately();
	}

	// Face the target
	Character->SetActorRotation(DirectionToTarget.Rotation());

	// Play warp montage if available
	UAnimMontage* Montage = WarpMontage.LoadSynchronous();
	if (Montage)
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(Montage);
		}
	}

	// Start tick timer for warp interpolation
	constexpr float TickRate = 1.0f / 60.0f;
	Character->GetWorldTimerManager().SetTimer(
		WarpTickHandle,
		FTimerDelegate::CreateUObject(this, &UGA_WarpStrike::TickWarp),
		TickRate,
		true
	);
}

void UGA_WarpStrike::TickWarp()
{
	ACharacter* Character = GetCombatCharacter();
	if (!Character || !WarpTarget.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Update end location in case target moved
	const FVector DirectionToTarget = (WarpTarget->GetActorLocation() - WarpStartLocation).GetSafeNormal();
	WarpEndLocation = WarpTarget->GetActorLocation() - DirectionToTarget * ArrivalOffset;

	WarpElapsed += GetWorld()->GetDeltaSeconds();
	const float Alpha = FMath::Clamp(WarpElapsed / WarpDuration, 0.0f, 1.0f);
	const float EasedAlpha = EaseInExpo(Alpha, WarpEaseExponent);

	// Interpolate position
	const FVector NewLocation = FMath::Lerp(WarpStartLocation, WarpEndLocation, EasedAlpha);
	Character->SetActorLocation(NewLocation);

	// Face target continuously
	Character->SetActorRotation(
		(WarpTarget->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal().Rotation()
	);

	// Check arrival
	if (Alpha >= 1.0f)
	{
		Character->GetWorldTimerManager().ClearTimer(WarpTickHandle);
		OnWarpArrived();
	}
}

void UGA_WarpStrike::OnWarpArrived()
{
	ACharacter* Character = GetCombatCharacter();
	AActor* Target = WarpTarget.Get();

	if (!Character || !Target)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Restore movement
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	// Apply damage
	UCombatAttackData* AttackData = WarpAttackData.LoadSynchronous();
	if (AttackData)
	{
		PlayAttackMontage(AttackData, Target);
		ApplyDamageToTarget(Target, AttackData);
	}

	// Apply knockback
	if (WarpKnockbackForce > 0.0f)
	{
		const FVector KnockbackDir = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal();
		ApplyKnockback(Target, WarpKnockbackForce, KnockbackDir);
	}

	// Camera shake
	if (LandingCameraShake)
	{
		APlayerController* PC = Cast<APlayerController>(Character->GetController());
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraShake(LandingCameraShake);
		}
	}

	// Send warp landed event
	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (ASC)
	{
		FGameplayEventData EventData;
		EventData.Instigator = Character;
		EventData.Target = Target;
		ASC->HandleGameplayEvent(CombatGameplayTags::Event_Warp_Landed, &EventData);
	}

	// Ability will end via OnMontageEnded from base class (if attack montage was played)
	// If no attack data, end now
	if (!AttackData)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

float UGA_WarpStrike::EaseInExpo(float T, float Exponent)
{
	return FMath::Pow(T, Exponent);
}
