#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CombatTypes.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;

// ============================================================================
// Enums
// ============================================================================

UENUM(BlueprintType)
enum class EAttackType : uint8
{
	Light,
	Heavy,
	Air,
	Counter,
	Special
};

UENUM(BlueprintType)
enum class EChargeQuality : uint8
{
	Miss,
	Half,
	Full
};

UENUM(BlueprintType)
enum class ETargetingMode : uint8
{
	/** Auto-switch based on input direction + distance */
	SoftLock,
	/** Manual lock-on, stays on target until released */
	HardLock
};

// ============================================================================
// Delegates
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetChanged, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatHit, AActor*, Target, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChargeReleased, float, ChargeAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatEvent);
