#include "Components/CombatHitStopComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UCombatHitStopComponent::UCombatHitStopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatHitStopComponent::ApplyHitStop(float Duration, float TimeDilation)
{
	if (!bEnableHitStop || bLocalHitStopActive)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const float UseDuration = (Duration < 0.f) ? DefaultHitStopDuration : Duration;
	const float UseDilation = (TimeDilation < 0.f) ? DefaultTimeDilation : TimeDilation;

	bLocalHitStopActive = true;
	Owner->CustomTimeDilation = UseDilation;

	GetWorld()->GetTimerManager().SetTimer(
		LocalHitStopTimerHandle,
		this,
		&UCombatHitStopComponent::RestoreLocalTimeDilation,
		UseDuration,
		false
	);
}

void UCombatHitStopComponent::ApplyGlobalHitStop(float Duration, float TimeDilation)
{
	if (!bEnableHitStop || bGlobalHitStopActive)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float UseDuration = (Duration < 0.f) ? GlobalHitStopDuration : Duration;
	const float UseDilation = (TimeDilation < 0.f) ? GlobalTimeDilation : TimeDilation;

	bGlobalHitStopActive = true;
	UGameplayStatics::SetGlobalTimeDilation(World, UseDilation);

	World->GetTimerManager().SetTimer(
		GlobalHitStopTimerHandle,
		this,
		&UCombatHitStopComponent::RestoreGlobalTimeDilation,
		UseDuration,
		false
	);
}

void UCombatHitStopComponent::RestoreLocalTimeDilation()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->CustomTimeDilation = 1.0f;
	}
	bLocalHitStopActive = false;
}

void UCombatHitStopComponent::RestoreGlobalTimeDilation()
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
	bGlobalHitStopActive = false;
}
