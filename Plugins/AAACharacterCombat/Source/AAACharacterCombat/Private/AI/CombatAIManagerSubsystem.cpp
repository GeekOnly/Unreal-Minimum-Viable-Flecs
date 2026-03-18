#include "AI/CombatAIManagerSubsystem.h"
#include "Core/CombatGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UCombatAIManagerSubsystem::UCombatAIManagerSubsystem()
{
}

bool UCombatAIManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

void UCombatAIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UCombatAIManagerSubsystem::Deinitialize()
{
	StopAILoop();
	RegisteredEnemies.Empty();
	Super::Deinitialize();
}

// -----------------------------------------------------------------------------
// AI Loop Control
// -----------------------------------------------------------------------------

void UCombatAIManagerSubsystem::StartAILoop()
{
	if (bAILoopActive)
		return;

	bAILoopActive = true;
	AILoopTick();
}

void UCombatAIManagerSubsystem::StopAILoop()
{
	bAILoopActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AILoopTimerHandle);
		World->GetTimerManager().ClearTimer(PostAttackTimerHandle);
	}
}

// -----------------------------------------------------------------------------
// Enemy Registration
// -----------------------------------------------------------------------------

void UCombatAIManagerSubsystem::RegisterEnemy(AActor* Enemy)
{
	if (!Enemy)
		return;

	// Avoid duplicates
	for (const TWeakObjectPtr<AActor>& Existing : RegisteredEnemies)
	{
		if (Existing.Get() == Enemy)
			return;
	}

	RegisteredEnemies.Add(Enemy);
}

void UCombatAIManagerSubsystem::UnregisterEnemy(AActor* Enemy)
{
	RegisteredEnemies.RemoveAll([Enemy](const TWeakObjectPtr<AActor>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Enemy;
	});

	if (LastAttacker.Get() == Enemy)
	{
		LastAttacker.Reset();
	}
}

// -----------------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------------

int32 UCombatAIManagerSubsystem::GetAliveEnemyCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& EnemyPtr : RegisteredEnemies)
	{
		AActor* Enemy = EnemyPtr.Get();
		if (!Enemy)
			continue;

		UAbilitySystemComponent* ASC = GetEnemyASC(Enemy);
		if (ASC && !ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Dead))
		{
			Count++;
		}
	}
	return Count;
}

AActor* UCombatAIManagerSubsystem::GetRandomEnemy() const
{
	return GetRandomEnemyExcluding(nullptr);
}

AActor* UCombatAIManagerSubsystem::GetRandomEnemyExcluding(AActor* ExcludedEnemy) const
{
	TArray<AActor*> Available;
	for (const TWeakObjectPtr<AActor>& EnemyPtr : RegisteredEnemies)
	{
		AActor* Enemy = EnemyPtr.Get();
		if (!Enemy || Enemy == ExcludedEnemy)
			continue;

		if (IsEnemyAvailable(Enemy))
		{
			Available.Add(Enemy);
		}
	}

	if (Available.Num() == 0)
		return nullptr;

	return Available[FMath::RandRange(0, Available.Num() - 1)];
}

bool UCombatAIManagerSubsystem::IsAnyEnemyPreparingAttack() const
{
	for (const TWeakObjectPtr<AActor>& EnemyPtr : RegisteredEnemies)
	{
		AActor* Enemy = EnemyPtr.Get();
		if (!Enemy)
			continue;

		UAbilitySystemComponent* ASC = GetEnemyASC(Enemy);
		if (ASC && ASC->HasMatchingGameplayTag(CombatGameplayTags::AI_State_PreparingAttack))
		{
			return true;
		}
	}
	return false;
}

// -----------------------------------------------------------------------------
// AI Loop Internals
// -----------------------------------------------------------------------------

void UCombatAIManagerSubsystem::AILoopTick()
{
	if (!bAILoopActive)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	// If someone is already preparing or attacking, wait and retry
	if (IsAnyEnemyPreparingAttack())
	{
		World->GetTimerManager().SetTimer(
			AILoopTimerHandle, this, &UCombatAIManagerSubsystem::AILoopTick,
			0.2f, false);
		return;
	}

	// Pick a random available enemy, preferring one that isn't the last attacker
	AActor* ChosenEnemy = GetRandomEnemyExcluding(LastAttacker.Get());
	if (!ChosenEnemy)
	{
		// Fall back: allow the last attacker if it's the only option
		ChosenEnemy = GetRandomEnemy();
	}

	if (ChosenEnemy)
	{
		// Set AI_State_PreparingAttack tag on chosen enemy's ASC
		UAbilitySystemComponent* ASC = GetEnemyASC(ChosenEnemy);
		if (ASC)
		{
			ASC->AddLooseGameplayTag(CombatGameplayTags::AI_State_PreparingAttack);
			LastAttacker = ChosenEnemy;
		}
	}

	// Schedule the next loop iteration after a random interval + post-attack delay
	const float Interval = FMath::RandRange(MinAttackInterval, MaxAttackInterval);
	World->GetTimerManager().SetTimer(
		PostAttackTimerHandle, this, &UCombatAIManagerSubsystem::OnPostAttackDelayFinished,
		Interval + PostAttackDelay, false);
}

void UCombatAIManagerSubsystem::OnPostAttackDelayFinished()
{
	if (!bAILoopActive)
		return;

	// Clear preparing/attacking tags on the last attacker and set retreating
	if (AActor* Attacker = LastAttacker.Get())
	{
		UAbilitySystemComponent* ASC = GetEnemyASC(Attacker);
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_PreparingAttack);
			ASC->RemoveLooseGameplayTag(CombatGameplayTags::AI_State_Attacking);
			ASC->AddLooseGameplayTag(CombatGameplayTags::AI_State_Retreating);
		}
	}

	// Continue the loop
	AILoopTick();
}

bool UCombatAIManagerSubsystem::IsEnemyAvailable(AActor* Enemy) const
{
	if (!Enemy)
		return false;

	UAbilitySystemComponent* ASC = GetEnemyASC(Enemy);
	if (!ASC)
		return false;

	// Not available if dead, stunned, retreating, already attacking, or preparing
	if (ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Dead))
		return false;
	if (ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Stunned))
		return false;
	if (ASC->HasMatchingGameplayTag(CombatGameplayTags::AI_State_Retreating))
		return false;
	if (ASC->HasMatchingGameplayTag(CombatGameplayTags::AI_State_Attacking))
		return false;
	if (ASC->HasMatchingGameplayTag(CombatGameplayTags::AI_State_PreparingAttack))
		return false;

	return true;
}

UAbilitySystemComponent* UCombatAIManagerSubsystem::GetEnemyASC(AActor* Enemy) const
{
	if (!Enemy)
		return nullptr;

	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
}
