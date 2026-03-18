#include "Components/CombatMovementComponent.h"
#include "Core/CombatGameplayTags.h"
#include "GAS/CombatAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UCombatMovementComponent::UCombatMovementComponent()
	: MovementSpeed(600.0f)
	, RotationSpeed(720.0f)
	, RunAcceleration(1.5f)
	, BoostMultiplier(2.0f)
	, BoostDuration(0.5f)
	, bIsRunning(false)
	, bIsBoosting(false)
	, CurrentInput(FVector2D::ZeroVector)
	, bMovementEnabled(true)
	, CurrentSpeedMultiplier(1.0f)
	, BoostTimeRemaining(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		CachedMovementComp = Character->GetCharacterMovement();
		CachedWarpingComp = Character->FindComponentByClass<UMotionWarpingComponent>();
	}
}

void UCombatMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMovementEnabled || !GetOwner())
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = CachedMovementComp.Get();
	if (!MovementComp)
	{
		return;
	}

	// --- Boost timer ---
	if (bIsBoosting)
	{
		BoostTimeRemaining -= DeltaTime;
		if (BoostTimeRemaining <= 0.0f)
		{
			bIsBoosting = false;
			BoostTimeRemaining = 0.0f;
		}
	}

	// --- Calculate target speed multiplier ---
	float TargetMultiplier = 1.0f;
	if (bIsBoosting)
	{
		TargetMultiplier = BoostMultiplier;
	}
	else if (bIsRunning)
	{
		TargetMultiplier = RunAcceleration;
	}

	// Smooth acceleration lerp
	CurrentSpeedMultiplier = FMath::FInterpTo(CurrentSpeedMultiplier, TargetMultiplier, DeltaTime, 8.0f);

	// --- Apply camera-relative movement ---
	if (!CurrentInput.IsNearlyZero())
	{
		const FVector WorldDirection = GetCameraRelativeDirection();
		const float FinalSpeed = MovementSpeed * CurrentSpeedMultiplier;

		MovementComp->MaxWalkSpeed = FinalSpeed;
		MovementComp->AddInputVector(WorldDirection);

		// Rotate towards movement direction
		RotateTowards(WorldDirection, DeltaTime);
	}
	else
	{
		// Decelerate to base speed when no input
		MovementComp->MaxWalkSpeed = FMath::FInterpTo(MovementComp->MaxWalkSpeed, MovementSpeed, DeltaTime, 5.0f);
	}
}

void UCombatMovementComponent::SetMovementInput(FVector2D Input)
{
	CurrentInput = Input;
}

void UCombatMovementComponent::SetMovementEnabled(bool bEnabled)
{
	bMovementEnabled = bEnabled;
	if (!bEnabled)
	{
		CurrentInput = FVector2D::ZeroVector;
	}
}

void UCombatMovementComponent::RotateTowards(FVector TargetDirection, float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || TargetDirection.IsNearlyZero())
	{
		return;
	}

	TargetDirection.Z = 0.0f;
	TargetDirection.Normalize();

	const FRotator CurrentRotation = Owner->GetActorRotation();
	const FRotator TargetRotation = TargetDirection.Rotation();
	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed / 90.0f);

	Owner->SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
}

void UCombatMovementComponent::RotateToCamera()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn)
	{
		return;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	CameraForward.Z = 0.0f;
	CameraForward.Normalize();

	Owner->SetActorRotation(CameraForward.Rotation());
}

void UCombatMovementComponent::SetRunning(bool bRun)
{
	bIsRunning = bRun;
}

void UCombatMovementComponent::ActivateBoost()
{
	// Check if we have boost available from GAS
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (const UCombatAttributeSet* Attributes = ASC->GetSet<UCombatAttributeSet>())
		{
			if (Attributes->GetBoostAmount() <= 0.0f)
			{
				return; // No boost available
			}
		}
	}

	bIsBoosting = true;
	BoostTimeRemaining = BoostDuration;
	OnBoostActivated.Broadcast();
}

FVector UCombatMovementComponent::GetCameraRelativeDirection() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return FVector::ForwardVector;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return FVector::ForwardVector;
	}

	const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	const FRotator YawOnly(0.0f, CameraRotation.Yaw, 0.0f);

	const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

	return (Forward * CurrentInput.Y + Right * CurrentInput.X).GetSafeNormal();
}
