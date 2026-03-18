#include "UI/CombatDamageNumberComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCombatDamageNumberComponent::UCombatDamageNumberComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatDamageNumberComponent::SpawnDamageNumber(float Damage, FVector WorldLocation, bool bIsCritical)
{
	UWorld* World = GetWorld();
	if (!World || !DamageNumberWidgetClass)
	{
		return;
	}

	// Apply random spread offset
	const float OffsetX = FMath::FRandRange(-RandomSpread.X, RandomSpread.X);
	const float OffsetY = FMath::FRandRange(-RandomSpread.Y, RandomSpread.Y);
	WorldLocation += FVector(OffsetX, OffsetY, 0.f);

	// Spawn a temporary actor to hold the widget component
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = GetOwner();

	AActor* TempActor = World->SpawnActor<AActor>(AActor::StaticClass(), WorldLocation, FRotator::ZeroRotator, SpawnParams);
	if (!TempActor)
	{
		return;
	}

	// Add a scene root so the widget component can attach
	USceneComponent* SceneRoot = NewObject<USceneComponent>(TempActor, TEXT("SceneRoot"));
	TempActor->SetRootComponent(SceneRoot);
	SceneRoot->RegisterComponent();

	// Create and configure the widget component
	UWidgetComponent* WidgetComp = NewObject<UWidgetComponent>(TempActor, TEXT("DamageNumberWidget"));
	WidgetComp->SetupAttachment(SceneRoot);
	WidgetComp->SetWidgetClass(DamageNumberWidgetClass);
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComp->SetDrawSize(FVector2D(200.f, 50.f));
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComp->RegisterComponent();

	// Destroy the temporary actor after FloatDuration
	TWeakObjectPtr<AActor> WeakTempActor = TempActor;
	const float Duration = FloatDuration;

	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(
		TimerHandle,
		[WeakTempActor]()
		{
			if (WeakTempActor.IsValid())
			{
				WeakTempActor->Destroy();
			}
		},
		Duration,
		false
	);
}
