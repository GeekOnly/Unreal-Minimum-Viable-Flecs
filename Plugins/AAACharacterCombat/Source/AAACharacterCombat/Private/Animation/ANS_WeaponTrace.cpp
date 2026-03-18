#include "Animation/ANS_WeaponTrace.h"
#include "Core/CombatGameplayTags.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameplayCueManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UANS_WeaponTrace::UANS_WeaponTrace()
	: TraceStartSocket(NAME_None)
	, TraceEndSocket(NAME_None)
	, TraceRadius(20.0f)
	, TraceChannel(ECC_Pawn)
	, bDebugDraw(false)
	, PreviousStartLocation(FVector::ZeroVector)
	, PreviousEndLocation(FVector::ZeroVector)
{
}

FString UANS_WeaponTrace::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Weapon Trace [%s -> %s]"), *TraceStartSocket.ToString(), *TraceEndSocket.ToString());
}

void UANS_WeaponTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Store initial socket positions
	PreviousStartLocation = GetMeshSocketLocation(MeshComp, TraceStartSocket);
	PreviousEndLocation = GetMeshSocketLocation(MeshComp, TraceEndSocket);

	// Clear the hit list for this new trace window
	AlreadyHitActors.Empty();
}

void UANS_WeaponTrace::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp)
	{
		return;
	}

	PerformTrace(MeshComp);
}

void UANS_WeaponTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AlreadyHitActors.Empty();
	PreviousStartLocation = FVector::ZeroVector;
	PreviousEndLocation = FVector::ZeroVector;
}

FVector UANS_WeaponTrace::GetMeshSocketLocation(USkeletalMeshComponent* MeshComp, const FName& SocketName) const
{
	if (MeshComp && SocketName != NAME_None)
	{
		return MeshComp->GetSocketLocation(SocketName);
	}
	return MeshComp ? MeshComp->GetComponentLocation() : FVector::ZeroVector;
}

void UANS_WeaponTrace::PerformTrace(USkeletalMeshComponent* MeshComp)
{
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector CurrentStart = GetMeshSocketLocation(MeshComp, TraceStartSocket);
	const FVector CurrentEnd = GetMeshSocketLocation(MeshComp, TraceEndSocket);

	// Build trace params
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerActor);
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;

	// We perform multiple sweeps along the weapon to cover the full blade length
	// Sweep from previous start to current start
	// Sweep from previous end to current end
	// This creates a "ribbon" of traces covering the weapon's arc

	TArray<FHitResult> HitResults;

	// Sweep along the start socket path
	World->SweepMultiByChannel(
		HitResults,
		PreviousStartLocation,
		CurrentStart,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams
	);

	// Sweep along the end socket path
	TArray<FHitResult> EndHits;
	World->SweepMultiByChannel(
		EndHits,
		PreviousEndLocation,
		CurrentEnd,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams
	);
	HitResults.Append(EndHits);

	// Sweep between current start and current end (cross-section)
	TArray<FHitResult> CrossHits;
	World->SweepMultiByChannel(
		CrossHits,
		CurrentStart,
		CurrentEnd,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams
	);
	HitResults.Append(CrossHits);

#if ENABLE_DRAW_DEBUG
	if (bDebugDraw)
	{
		DrawDebugLine(World, PreviousStartLocation, CurrentStart, FColor::Green, false, 1.0f);
		DrawDebugLine(World, PreviousEndLocation, CurrentEnd, FColor::Red, false, 1.0f);
		DrawDebugLine(World, CurrentStart, CurrentEnd, FColor::Blue, false, 1.0f);
		DrawDebugSphere(World, CurrentStart, TraceRadius, 8, FColor::Green, false, 1.0f);
		DrawDebugSphere(World, CurrentEnd, TraceRadius, 8, FColor::Red, false, 1.0f);
	}
#endif

	// Process hits
	UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == OwnerActor)
		{
			continue;
		}

		// Skip if already hit this actor during this trace window
		TWeakObjectPtr<AActor> WeakHitActor(HitActor);
		if (AlreadyHitActors.Contains(WeakHitActor))
		{
			continue;
		}

		AlreadyHitActors.Add(WeakHitActor);

#if ENABLE_DRAW_DEBUG
		if (bDebugDraw)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, TraceRadius * 1.5f, 12, FColor::Yellow, false, 2.0f);
		}
#endif

		// Send gameplay event through ASC
		if (OwnerASC)
		{
			FGameplayEventData EventData;
			EventData.Instigator = OwnerActor;
			EventData.Target = HitActor;
			EventData.ContextHandle.AddHitResult(Hit);

			OwnerASC->HandleGameplayEvent(CombatGameplayTags::Event_Combat_Hit, &EventData);

			// Trigger GameplayCue for hit impact VFX/SFX
			FGameplayCueParameters CueParams;
			CueParams.EffectContext = OwnerASC->MakeEffectContext();
			CueParams.EffectContext.AddHitResult(Hit);
			CueParams.Location = Hit.ImpactPoint;
			CueParams.Normal = Hit.ImpactNormal;
			OwnerASC->ExecuteGameplayCue(CombatGameplayTags::GameplayCue_Combat_HitImpact, CueParams);
		}
	}

	// Update previous positions for next tick
	PreviousStartLocation = CurrentStart;
	PreviousEndLocation = CurrentEnd;
}
