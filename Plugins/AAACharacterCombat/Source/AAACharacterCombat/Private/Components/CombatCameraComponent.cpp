#include "Components/CombatCameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/KismetMathLibrary.h"

UCombatCameraComponent::UCombatCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCombatCameraComponent::SetLockOnTarget(AActor* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	LockOnTarget = Target;
	bIsLockedOn = true;
	SetComponentTickEnabled(true);
}

void UCombatCameraComponent::ClearLockOnTarget()
{
	LockOnTarget.Reset();
	bIsLockedOn = false;
	SetComponentTickEnabled(false);

	// Restore default FOV
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
			{
				CamMgr->SetFOV(DefaultFOV);
			}
		}
	}
}

void UCombatCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickCameraUpdate(DeltaTime);
}

void UCombatCameraComponent::TickCameraUpdate(float DeltaTime)
{
	if (!LockOnTarget.IsValid())
	{
		ClearLockOnTarget();
		return;
	}

	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	// Calculate desired rotation toward target
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector TargetLocation = LockOnTarget->GetActorLocation();
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(OwnerLocation, TargetLocation);

	// Apply pitch offset
	FRotator DesiredRotation = LookAtRotation;
	DesiredRotation.Pitch += LockOnPitchOffset;

	// Interpolate controller rotation toward target
	const FRotator CurrentRotation = PC->GetControlRotation();
	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaTime, LockOnInterpSpeed);
	PC->SetControlRotation(NewRotation);

	// Interpolate FOV toward lock-on FOV
	if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
	{
		const float CurrentFOV = CamMgr->GetFOVAngle();
		const float NewFOV = FMath::FInterpTo(CurrentFOV, LockOnFOV, DeltaTime, FOVInterpSpeed);
		CamMgr->SetFOV(NewFOV);
	}
}
