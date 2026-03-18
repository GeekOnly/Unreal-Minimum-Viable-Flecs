#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "CombatCharacterBase.generated.h"

class UCombatAbilitySystemComponent;
class UCombatAttributeSet;
class UMotionWarpingComponent;
class UTargetingComponent;
class UCombatMovementComponent;
class UBoostSystemComponent;
class UCombatHitStopComponent;
class UCombatCameraComponent;
class UCombatDamageNumberComponent;
class UGameplayAbility;
class UGameplayEffect;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * AAA-grade player combat character base.
 * Integrates GAS, MotionWarping, Targeting, CombatMovement, and Boost systems.
 * Uses EnhancedInput for action binding that routes through GAS tags.
 */
UCLASS()
class AAACHARACTERCOMBAT_API ACombatCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACombatCharacterBase();

	// --- IAbilitySystemInterface ---
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Get the typed combat ASC */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UCombatAbilitySystemComponent* GetCombatAbilitySystemComponent() const { return AbilitySystemComp; }

	/** Get the targeting component */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UTargetingComponent* GetTargetingComponent() const { return TargetingComp; }

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UCombatAbilitySystemComponent> AbilitySystemComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UTargetingComponent> TargetingComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UCombatMovementComponent> MovementComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UBoostSystemComponent> BoostComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UCombatHitStopComponent> HitStopComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UCombatCameraComponent> CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components")
	TObjectPtr<UCombatDamageNumberComponent> DamageNumberComp;

	// --- GAS Data ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|GAS")
	TObjectPtr<UCombatAttributeSet> AttributeSet;

	/** Abilities granted at startup */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Effects applied at startup (e.g., base stat initialization) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|GAS")
	TArray<TSubclassOf<UGameplayEffect>> DefaultEffects;

	// --- Enhanced Input ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_HeavyAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_Counter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_Dodge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_WarpStrike;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_WeaponThrow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_ProjectileShot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> IA_Run;

private:
	/** Initialize GAS: grant abilities, apply effects, create attribute set */
	void InitializeAbilitySystem();

	// --- Input Handlers ---
	void HandleAttack(const FInputActionValue& Value);
	void HandleHeavyAttack(const FInputActionValue& Value);
	void HandleCounter(const FInputActionValue& Value);
	void HandleDodge(const FInputActionValue& Value);
	void HandleWarpStrike(const FInputActionValue& Value);
	void HandleWeaponThrow(const FInputActionValue& Value);
	void HandleProjectileShot(const FInputActionValue& Value);
	void HandleMove(const FInputActionValue& Value);
	void HandleRunStarted(const FInputActionValue& Value);
	void HandleRunCompleted(const FInputActionValue& Value);
};
