#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "CombatAIManagerSubsystem.generated.h"

class UAbilitySystemComponent;

/**
 * World-level AI coordinator for Batman Arkham-style combat.
 * Manages the attack queue: picks which enemy attacks next,
 * enforces cooldowns, and drives AI state via GAS tags.
 */
UCLASS()
class AAACHARACTERCOMBAT_API UCombatAIManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCombatAIManagerSubsystem();

	// --- UWorldSubsystem interface ---
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- AI Loop Control ---

	/** Start the coordinated attack loop */
	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	void StartAILoop();

	/** Stop the coordinated attack loop */
	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	void StopAILoop();

	// --- Enemy Registration ---

	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	void RegisterEnemy(AActor* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Combat|AI")
	void UnregisterEnemy(AActor* Enemy);

	// --- Queries ---

	/** Returns the number of alive enemies (those without State_Combat_Dead tag) */
	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	int32 GetAliveEnemyCount() const;

	/** Get a random alive, available enemy */
	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	AActor* GetRandomEnemy() const;

	/** Get a random alive, available enemy excluding the specified actor */
	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	AActor* GetRandomEnemyExcluding(AActor* ExcludedEnemy) const;

	/** Returns true if any registered enemy has the AI_State_PreparingAttack tag */
	UFUNCTION(BlueprintPure, Category = "Combat|AI")
	bool IsAnyEnemyPreparingAttack() const;

protected:
	/** Minimum time between coordinated attacks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|AI", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float MinAttackInterval = 0.5f;

	/** Maximum time between coordinated attacks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|AI", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float MaxAttackInterval = 1.5f;

	/** Delay after an enemy finishes attacking before the next attack is queued */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|AI", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float PostAttackDelay = 0.3f;

private:
	/** Timer-driven AI loop step: pick an enemy and assign attack */
	void AILoopTick();

	/** Called after post-attack delay to re-enter the loop */
	void OnPostAttackDelayFinished();

	/** Check if an enemy is available to attack (alive, not stunned, not retreating, not already attacking) */
	bool IsEnemyAvailable(AActor* Enemy) const;

	/** Get the ASC from an enemy actor */
	UAbilitySystemComponent* GetEnemyASC(AActor* Enemy) const;

	/** All registered enemies */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredEnemies;

	/** The last enemy that attacked (to avoid repeats) */
	TWeakObjectPtr<AActor> LastAttacker;

	/** Timer handles for the AI loop */
	FTimerHandle AILoopTimerHandle;
	FTimerHandle PostAttackTimerHandle;

	/** Whether the AI loop is currently running */
	bool bAILoopActive = false;
};
