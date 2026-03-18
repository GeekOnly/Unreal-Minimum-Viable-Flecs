#include "VFX/GC_HitImpact.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

UGC_HitImpact::UGC_HitImpact()
{
}

bool UGC_HitImpact::HandlesEvent(EGameplayCueEvent EventType) const
{
	return EventType == EGameplayCueEvent::Executed;
}

void UGC_HitImpact::HandleGameplayCue(
	AActor* MyTarget,
	EGameplayCueEvent EventType,
	const FGameplayCueParameters& Parameters)
{
	if (EventType == EGameplayCueEvent::Executed)
	{
		OnExecute_Implementation(MyTarget, Parameters);
	}
}

bool UGC_HitImpact::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return false;
	}

	UWorld* World = MyTarget->GetWorld();
	if (!World)
	{
		return false;
	}

	// --- Determine hit location ---
	FVector HitLocation = MyTarget->GetActorLocation();
	FRotator HitRotation = FRotator::ZeroRotator;

	if (Parameters.EffectContext.IsValid())
	{
		if (const FHitResult* HitResult = Parameters.EffectContext.GetHitResult())
		{
			HitLocation = HitResult->ImpactPoint;
			HitRotation = HitResult->ImpactNormal.Rotation();
		}
		else if (!Parameters.EffectContext.GetOrigin().IsZero())
		{
			HitLocation = Parameters.EffectContext.GetOrigin();
		}
	}

	// --- Spawn Niagara VFX ---
	if (UNiagaraSystem* VFXSystem = ImpactVFX.LoadSynchronous())
	{
		if (bAttachToTarget)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				VFXSystem,
				MyTarget->GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				VFXScale,
				EAttachLocation::KeepRelativeOffset,
				true,
				ENCPoolMethod::None
			);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				VFXSystem,
				HitLocation,
				HitRotation,
				VFXScale,
				true,
				true,
				ENCPoolMethod::None
			);
		}
	}

	// --- Play impact sound ---
	if (USoundBase* Sound = ImpactSFX.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, HitLocation);
	}

	// --- Camera shake ---
	if (CameraShakeClass)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->ClientStartCameraShake(CameraShakeClass, CameraShakeScale);
		}
	}

	return true;
}
