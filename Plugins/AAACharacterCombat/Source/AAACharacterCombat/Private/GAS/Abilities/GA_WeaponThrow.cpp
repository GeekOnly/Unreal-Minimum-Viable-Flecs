#include "GAS/Abilities/GA_WeaponThrow.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "Components/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UGA_WeaponThrow::UGA_WeaponThrow()
{
	AbilityTags.AddTag(CombatGameplayTags::Ability_WeaponThrow);

	// Block while weapon is already being recalled
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Weapon_Returning);
}

void UGA_WeaponThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	if (!WeaponActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (IsWeaponInFlight())
	{
		ExecuteRecall();
	}
	else
	{
		ExecuteThrow();
	}
}

void UGA_WeaponThrow::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const ACharacter* Character = GetCombatCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(RecallTickHandle);
	}

	RecallAlpha = 0.0f;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_WeaponThrow::IsWeaponInFlight() const
{
	const UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (!ASC)
		return false;

	return ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Weapon_InFlight);
}

void UGA_WeaponThrow::ExecuteThrow()
{
	ACharacter* Character = GetCombatCharacter();
	AActor* Weapon = WeaponActor.Get();
	if (!Character || !Weapon)
		return;

	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (!ASC)
		return;

	// Play throw montage
	UAnimMontage* Montage = ThrowMontage.LoadSynchronous();
	if (Montage)
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(Montage);
		}
	}

	// Detach weapon from character
	Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// Enable physics on the weapon's root primitive
	UPrimitiveComponent* WeaponPrimitive = Cast<UPrimitiveComponent>(Weapon->GetRootComponent());
	if (WeaponPrimitive)
	{
		WeaponPrimitive->SetSimulatePhysics(true);
		WeaponPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// Calculate throw direction from camera forward
		const APlayerController* PC = Cast<APlayerController>(Character->GetController());
		FVector ThrowDirection = Character->GetActorForwardVector();
		if (PC && PC->PlayerCameraManager)
		{
			ThrowDirection = PC->PlayerCameraManager->GetActorForwardVector();
		}

		WeaponPrimitive->AddImpulse(ThrowDirection * ThrowPower, NAME_None, true);
	}

	// Add weapon in-flight tag
	ASC->AddLooseGameplayTag(CombatGameplayTags::State_Weapon_InFlight);
	ASC->RemoveLooseGameplayTag(CombatGameplayTags::State_Weapon_InHand);

	// Send thrown event
	FGameplayEventData EventData;
	EventData.Instigator = Character;
	ASC->HandleGameplayEvent(CombatGameplayTags::Event_Weapon_Thrown, &EventData);

	// End ability after throw (recall is a separate activation)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_WeaponThrow::ExecuteRecall()
{
	ACharacter* Character = GetCombatCharacter();
	AActor* Weapon = WeaponActor.Get();
	if (!Character || !Weapon)
		return;

	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (!ASC)
		return;

	// Play recall montage
	UAnimMontage* Montage = RecallMontage.LoadSynchronous();
	if (Montage)
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(Montage);
		}
	}

	// Disable physics for controlled interpolation
	UPrimitiveComponent* WeaponPrimitive = Cast<UPrimitiveComponent>(Weapon->GetRootComponent());
	if (WeaponPrimitive)
	{
		WeaponPrimitive->SetSimulatePhysics(false);
		WeaponPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Update state tags
	ASC->RemoveLooseGameplayTag(CombatGameplayTags::State_Weapon_InFlight);
	ASC->AddLooseGameplayTag(CombatGameplayTags::State_Weapon_Returning);

	// Initialize recall state
	RecallStartLocation = Weapon->GetActorLocation();
	RecallAlpha = 0.0f;

	// Start recall tick
	constexpr float TickRate = 1.0f / 60.0f;
	Character->GetWorldTimerManager().SetTimer(
		RecallTickHandle,
		FTimerDelegate::CreateUObject(this, &UGA_WeaponThrow::TickRecall),
		TickRate,
		true
	);
}

void UGA_WeaponThrow::TickRecall()
{
	ACharacter* Character = GetCombatCharacter();
	AActor* Weapon = WeaponActor.Get();
	if (!Character || !Weapon)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FTransform HandTransform = GetHandSocketTransform();
	const FVector HandLocation = HandTransform.GetLocation();

	// Calculate total travel distance for speed-based alpha
	const float TotalDistance = FVector::Dist(RecallStartLocation, HandLocation);
	if (TotalDistance < KINDA_SMALL_NUMBER)
	{
		OnWeaponCaught();
		return;
	}

	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	const float AlphaStep = (RecallSpeed * DeltaTime) / TotalDistance;
	RecallAlpha = FMath::Clamp(RecallAlpha + AlphaStep, 0.0f, 1.0f);

	// Bezier curve: P0 = start, P1 = control point (elevated midpoint), P2 = hand
	const FVector MidPoint = (RecallStartLocation + HandLocation) * 0.5f;
	const FVector ControlPoint = MidPoint + FVector(0.0f, 0.0f, CurvePointOffset);
	const FVector NewLocation = GetQuadraticBezierPoint(RecallStartLocation, ControlPoint, HandLocation, RecallAlpha);

	Weapon->SetActorLocation(NewLocation);

	// Rotate weapon to face travel direction
	const FVector Velocity = (NewLocation - Weapon->GetActorLocation()).GetSafeNormal();
	if (!Velocity.IsNearlyZero())
	{
		Weapon->SetActorRotation(Velocity.Rotation());
	}

	// Check if caught
	const float DistanceToHand = FVector::Dist(NewLocation, HandLocation);
	if (DistanceToHand <= CatchDistance || RecallAlpha >= 1.0f)
	{
		OnWeaponCaught();
	}
}

void UGA_WeaponThrow::OnWeaponCaught()
{
	ACharacter* Character = GetCombatCharacter();
	AActor* Weapon = WeaponActor.Get();

	if (const ACharacter* Char = GetCombatCharacter())
	{
		Char->GetWorldTimerManager().ClearTimer(RecallTickHandle);
	}

	if (Character && Weapon)
	{
		// Reattach weapon to hand socket
		Weapon->AttachToComponent(
			Character->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			HandSocketName
		);

		// Update state tags
		UCombatAbilitySystemComponent* ASC = GetCombatASC();
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(CombatGameplayTags::State_Weapon_Returning);
			ASC->AddLooseGameplayTag(CombatGameplayTags::State_Weapon_InHand);

			// Send caught event
			FGameplayEventData EventData;
			EventData.Instigator = Character;
			ASC->HandleGameplayEvent(CombatGameplayTags::Event_Weapon_Caught, &EventData);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

FVector UGA_WeaponThrow::GetQuadraticBezierPoint(const FVector& P0, const FVector& P1, const FVector& P2, float T)
{
	// B(t) = (1-t)^2 * P0 + 2*(1-t)*t * P1 + t^2 * P2
	const float OneMinusT = 1.0f - T;
	return (OneMinusT * OneMinusT * P0) + (2.0f * OneMinusT * T * P1) + (T * T * P2);
}

FTransform UGA_WeaponThrow::GetHandSocketTransform() const
{
	const ACharacter* Character = GetCombatCharacter();
	if (Character && Character->GetMesh())
	{
		return Character->GetMesh()->GetSocketTransform(HandSocketName);
	}
	return FTransform::Identity;
}
