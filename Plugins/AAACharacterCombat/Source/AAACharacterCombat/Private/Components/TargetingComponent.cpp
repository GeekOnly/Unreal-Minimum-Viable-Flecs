#include "Components/TargetingComponent.h"
#include "Core/CombatGameplayTags.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UTargetingComponent::UTargetingComponent()
	: DetectionRange(2500.0f)
	, DotProductThreshold(0.3f)
	, ScreenDistanceWeight(0.6f)
	, PositionDistanceWeight(0.4f)
	, TargetingMode(ETargetingMode::SoftLock)
	, LastInputDirection(FVector::ForwardVector)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Clean up stale weak pointers
	RegisteredEnemies.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });

	// In SoftLock mode, automatically re-evaluate target
	if (TargetingMode == ETargetingMode::SoftLock)
	{
		AActor* BestTarget = FindTargetNearScreenCenter();
		if (BestTarget != CurrentTarget.Get())
		{
			SetCurrentTarget(BestTarget);
		}
	}
	else
	{
		// HardLock: verify current target is still valid
		if (CurrentTarget.IsValid())
		{
			AActor* Target = CurrentTarget.Get();
			if (ActorHasTag(Target, CombatGameplayTags::State_Combat_Dead))
			{
				ClearTarget();
			}
			else
			{
				// Check range
				const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation());
				if (Distance > DetectionRange * 1.5f)
				{
					ClearTarget();
				}
			}
		}
	}
}

// --- Registration ---

void UTargetingComponent::RegisterEnemy(AActor* Enemy)
{
	if (Enemy && !RegisteredEnemies.ContainsByPredicate([Enemy](const TWeakObjectPtr<AActor>& Ptr) { return Ptr.Get() == Enemy; }))
	{
		RegisteredEnemies.Add(Enemy);
	}
}

void UTargetingComponent::UnregisterEnemy(AActor* Enemy)
{
	RegisteredEnemies.RemoveAll([Enemy](const TWeakObjectPtr<AActor>& Ptr) { return Ptr.Get() == Enemy; });

	if (CurrentTarget.Get() == Enemy)
	{
		ClearTarget();
	}
}

// --- Target Access ---

AActor* UTargetingComponent::GetCurrentTarget() const
{
	return CurrentTarget.Get();
}

void UTargetingComponent::SetCurrentTarget(AActor* NewTarget)
{
	AActor* OldTarget = CurrentTarget.Get();
	if (OldTarget != NewTarget)
	{
		CurrentTarget = NewTarget;
		OnTargetChanged.Broadcast(NewTarget);
	}
}

void UTargetingComponent::ClearTarget()
{
	SetCurrentTarget(nullptr);
}

// --- Queries ---

AActor* UTargetingComponent::GetRandomEnemy() const
{
	TArray<AActor*> Alive = GetAliveEnemies();
	if (Alive.Num() == 0)
	{
		return nullptr;
	}
	return Alive[FMath::RandRange(0, Alive.Num() - 1)];
}

AActor* UTargetingComponent::GetRandomEnemyExcluding(AActor* Exclude) const
{
	TArray<AActor*> Alive = GetAliveEnemies();
	Alive.Remove(Exclude);
	if (Alive.Num() == 0)
	{
		return nullptr;
	}
	return Alive[FMath::RandRange(0, Alive.Num() - 1)];
}

AActor* UTargetingComponent::GetClosestAttackingEnemy() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	AActor* ClosestEnemy = nullptr;
	float ClosestDistance = MAX_FLT;

	for (const TWeakObjectPtr<AActor>& EnemyPtr : RegisteredEnemies)
	{
		AActor* Enemy = EnemyPtr.Get();
		if (!Enemy)
		{
			continue;
		}

		if (ActorHasTag(Enemy, CombatGameplayTags::State_Combat_Dead))
		{
			continue;
		}

		if (!ActorHasTag(Enemy, CombatGameplayTags::AI_State_PreparingAttack))
		{
			continue;
		}

		const float Distance = FVector::Dist(OwnerLocation, Enemy->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestEnemy = Enemy;
		}
	}

	return ClosestEnemy;
}

AActor* UTargetingComponent::FindTargetInDirection(FVector Direction)
{
	AActor* Owner = GetOwner();
	if (!Owner || Direction.IsNearlyZero())
	{
		return nullptr;
	}

	Direction.Normalize();
	LastInputDirection = Direction;

	const FVector OwnerLocation = Owner->GetActorLocation();
	TArray<AActor*> Alive = GetAliveEnemies();

	AActor* BestTarget = nullptr;
	float BestScore = -2.0f; // Dot product ranges from -1 to 1

	for (AActor* Enemy : Alive)
	{
		const FVector ToEnemy = (Enemy->GetActorLocation() - OwnerLocation);
		const float Distance = ToEnemy.Size();

		if (Distance > DetectionRange || Distance < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector DirectionToEnemy = ToEnemy / Distance;
		const float DotProduct = FVector::DotProduct(Direction, DirectionToEnemy);

		if (DotProduct < DotProductThreshold)
		{
			continue;
		}

		// Score: dot product weighted by inverse distance
		const float DistanceScore = 1.0f - (Distance / DetectionRange);
		const float Score = (DotProduct * 0.7f) + (DistanceScore * 0.3f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Enemy;
		}
	}

	return BestTarget;
}

AActor* UTargetingComponent::FindTargetNearScreenCenter()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	APlayerController* PC = Cast<APlayerController>(Cast<APawn>(Owner)->GetController());
	if (!PC)
	{
		return nullptr;
	}

	int32 ViewportSizeX, ViewportSizeY;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	const FVector2D ScreenCenter(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f);
	const FVector OwnerLocation = Owner->GetActorLocation();

	TArray<AActor*> Alive = GetAliveEnemies();

	AActor* BestTarget = nullptr;
	float BestScore = -MAX_FLT;

	for (AActor* Enemy : Alive)
	{
		const float WorldDistance = FVector::Dist(OwnerLocation, Enemy->GetActorLocation());
		if (WorldDistance > DetectionRange)
		{
			continue;
		}

		// Project enemy to screen
		FVector2D ScreenPos;
		if (!PC->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), ScreenPos))
		{
			continue;
		}

		// Screen distance score (inverted, closer to center = higher score)
		const float ScreenDist = FVector2D::Distance(ScreenCenter, ScreenPos);
		const float MaxScreenDist = FMath::Sqrt(FMath::Square(ViewportSizeX * 0.5f) + FMath::Square(ViewportSizeY * 0.5f));
		const float ScreenScore = 1.0f - FMath::Clamp(ScreenDist / MaxScreenDist, 0.0f, 1.0f);

		// World distance score (inverted, closer = higher score)
		const float DistanceScore = 1.0f - FMath::Clamp(WorldDistance / DetectionRange, 0.0f, 1.0f);

		const float FinalScore = (ScreenScore * ScreenDistanceWeight) + (DistanceScore * PositionDistanceWeight);

		if (FinalScore > BestScore)
		{
			BestScore = FinalScore;
			BestTarget = Enemy;
		}
	}

	return BestTarget;
}

int32 UTargetingComponent::GetAliveEnemyCount() const
{
	return GetAliveEnemies().Num();
}

bool UTargetingComponent::IsAnyEnemyPreparingAttack() const
{
	for (const TWeakObjectPtr<AActor>& EnemyPtr : RegisteredEnemies)
	{
		AActor* Enemy = EnemyPtr.Get();
		if (Enemy && ActorHasTag(Enemy, CombatGameplayTags::AI_State_PreparingAttack))
		{
			return true;
		}
	}
	return false;
}

void UTargetingComponent::ToggleLockOn()
{
	if (TargetingMode == ETargetingMode::SoftLock)
	{
		TargetingMode = ETargetingMode::HardLock;
		// Lock onto nearest screen-center target
		AActor* Target = FindTargetNearScreenCenter();
		SetCurrentTarget(Target);
	}
	else
	{
		TargetingMode = ETargetingMode::SoftLock;
		// SoftLock will re-evaluate on next tick
	}
}

// --- Private Helpers ---

TArray<AActor*> UTargetingComponent::GetAliveEnemies() const
{
	TArray<AActor*> Result;
	for (const TWeakObjectPtr<AActor>& EnemyPtr : RegisteredEnemies)
	{
		AActor* Enemy = EnemyPtr.Get();
		if (Enemy && !ActorHasTag(Enemy, CombatGameplayTags::State_Combat_Dead))
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}

bool UTargetingComponent::ActorHasTag(AActor* Actor, const FGameplayTag& Tag) const
{
	if (!Actor)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetASCFromActor(Actor);
	return ASC && ASC->HasMatchingGameplayTag(Tag);
}

UAbilitySystemComponent* UTargetingComponent::GetASCFromActor(AActor* Actor) const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
}
