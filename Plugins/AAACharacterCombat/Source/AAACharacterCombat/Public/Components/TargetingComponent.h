#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CombatTypes.h"
#include "TargetingComponent.generated.h"

class UAbilitySystemComponent;

/**
 * AAA-quality target detection and lock-on component.
 * Supports soft-lock (auto-switch based on input) and hard-lock (manual toggle) modes.
 * Uses GAS tags for enemy state queries rather than direct component lookups.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AAACHARACTERCOMBAT_API UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Configuration ---

	/** Maximum range to detect targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config", meta = (ClampMin = "0.0"))
	float DetectionRange;

	/** Dot-product threshold for direction-based targeting (-1 = behind, 1 = directly ahead) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DotProductThreshold;

	/** Weight of screen-center proximity in scoring (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScreenDistanceWeight;

	/** Weight of world-space distance in scoring (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PositionDistanceWeight;

	/** Current targeting mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config")
	ETargetingMode TargetingMode;

	// --- Registration ---

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void RegisterEnemy(AActor* Enemy);

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void UnregisterEnemy(AActor* Enemy);

	// --- Target Access ---

	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetCurrentTarget() const;

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetCurrentTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ClearTarget();

	// --- Queries ---

	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetRandomEnemy() const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetRandomEnemyExcluding(AActor* Exclude) const;

	/** Returns closest enemy that has AI_State_PreparingAttack tag active */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetClosestAttackingEnemy() const;

	/** Score and select the best target based on input direction (dot product) */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	AActor* FindTargetInDirection(FVector Direction);

	/** Score and select the best target based on screen-center proximity + world distance */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	AActor* FindTargetNearScreenCenter();

	/** Count of alive (non-dead-tagged) registered enemies */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	int32 GetAliveEnemyCount() const;

	/** Check if any registered enemy has AI_State_PreparingAttack tag */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsAnyEnemyPreparingAttack() const;

	/** Toggle between SoftLock and HardLock modes */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ToggleLockOn();

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category = "Targeting|Events")
	FOnTargetChanged OnTargetChanged;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredEnemies;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	/** Input direction for soft-lock auto-update */
	FVector LastInputDirection;

	/** Get alive enemies only (filters out dead-tagged actors) */
	TArray<AActor*> GetAliveEnemies() const;

	/** Check if an actor has a specific tag on its ASC */
	bool ActorHasTag(AActor* Actor, const struct FGameplayTag& Tag) const;

	/** Get ASC from an actor */
	UAbilitySystemComponent* GetASCFromActor(AActor* Actor) const;
};
