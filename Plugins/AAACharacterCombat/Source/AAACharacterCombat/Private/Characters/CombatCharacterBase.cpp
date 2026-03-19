#include "Characters/CombatCharacterBase.h"
#include "Core/CombatGameplayTags.h"
#include "Core/CombatTypes.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "Components/TargetingComponent.h"
#include "Components/CombatMovementComponent.h"
#include "Components/BoostSystemComponent.h"
#include "Components/CombatHitStopComponent.h"
#include "Components/CombatCameraComponent.h"
#include "UI/CombatDamageNumberComponent.h"
#include "MotionWarpingComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystemGlobals.h"

ACombatCharacterBase::ACombatCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create GAS component
	AbilitySystemComp = CreateDefaultSubobject<UCombatAbilitySystemComponent>(TEXT("AbilitySystemComp"));
	AbilitySystemComp->SetIsReplicated(true);
	AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create MotionWarping
	MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));

	// Create Targeting
	TargetingComp = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComp"));

	// Create Combat Movement
	MovementComp = CreateDefaultSubobject<UCombatMovementComponent>(TEXT("CombatMovementComp"));

	// Create Boost System
	BoostComp = CreateDefaultSubobject<UBoostSystemComponent>(TEXT("BoostComp"));

	// Create Hit Stop
	HitStopComp = CreateDefaultSubobject<UCombatHitStopComponent>(TEXT("HitStopComp"));

	// Create Combat Camera
	CameraComp = CreateDefaultSubobject<UCombatCameraComponent>(TEXT("CombatCameraComp"));

	// Create Damage Number
	DamageNumberComp = CreateDefaultSubobject<UCombatDamageNumberComponent>(TEXT("DamageNumberComp"));
}

UAbilitySystemComponent* ACombatCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void ACombatCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Add input mapping context
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	InitializeAbilitySystem();
}

void ACombatCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Re-initialize ability actor info on the server when possessed
	if (AbilitySystemComp)
	{
		AbilitySystemComp->InitAbilityActorInfo(this, this);
	}
}

void ACombatCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
		return;

	// Combat inputs - Triggered (on press)
	if (IA_Attack)
	{
		EnhancedInput->BindAction(IA_Attack, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleAttack);
	}
	if (IA_HeavyAttack)
	{
		EnhancedInput->BindAction(IA_HeavyAttack, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleHeavyAttack);
	}
	if (IA_Counter)
	{
		EnhancedInput->BindAction(IA_Counter, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleCounter);
	}
	if (IA_Dodge)
	{
		EnhancedInput->BindAction(IA_Dodge, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleDodge);
	}
	if (IA_WarpStrike)
	{
		EnhancedInput->BindAction(IA_WarpStrike, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleWarpStrike);
	}
	if (IA_WeaponThrow)
	{
		EnhancedInput->BindAction(IA_WeaponThrow, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleWeaponThrow);
	}
	if (IA_ProjectileShot)
	{
		EnhancedInput->BindAction(IA_ProjectileShot, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleProjectileShot);
	}

	// Movement - continuous
	if (IA_Move)
	{
		EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ACombatCharacterBase::HandleMove);
	}

	// Run - hold
	if (IA_Run)
	{
		EnhancedInput->BindAction(IA_Run, ETriggerEvent::Started, this, &ACombatCharacterBase::HandleRunStarted);
		EnhancedInput->BindAction(IA_Run, ETriggerEvent::Completed, this, &ACombatCharacterBase::HandleRunCompleted);
	}
}

// -----------------------------------------------------------------------------
// GAS Initialization
// -----------------------------------------------------------------------------

void ACombatCharacterBase::InitializeAbilitySystem()
{
	if (!AbilitySystemComp)
		return;

	// Initialize ability actor info (owner and avatar are both this character)
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	// Create and register attribute set
	if (!AttributeSet)
	{
		AttributeSet = NewObject<UCombatAttributeSet>(this);
		AbilitySystemComp->AddAttributeSetSubobject(AttributeSet.Get());
		AbilitySystemComp->ForceReplication();
	}

	// Only grant abilities on the server
	if (HasAuthority())
	{
		AbilitySystemComp->GrantDefaultAbilities(DefaultAbilities);

		// Apply default effects
		for (const TSubclassOf<UGameplayEffect>& EffectClass : DefaultEffects)
		{
			if (EffectClass)
			{
				AbilitySystemComp->ApplyEffectToSelf(EffectClass);
			}
		}
	}
}

// -----------------------------------------------------------------------------
// Input Handlers - Route through GAS tags
// -----------------------------------------------------------------------------

void ACombatCharacterBase::HandleAttack(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->TryActivateAbilityByTag(CombatGameplayTags::Ability_Melee_Light);
	}
}

void ACombatCharacterBase::HandleHeavyAttack(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->TryActivateAbilityByTag(CombatGameplayTags::Ability_Melee_Heavy);
	}
}

void ACombatCharacterBase::HandleCounter(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->TryActivateAbilityByTag(CombatGameplayTags::Ability_Counter);
	}
}

void ACombatCharacterBase::HandleDodge(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->TryActivateAbilityByTag(CombatGameplayTags::Ability_Dodge);
	}
}

void ACombatCharacterBase::HandleWarpStrike(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->TryActivateAbilityByTag(CombatGameplayTags::Ability_WarpStrike);
	}
}

void ACombatCharacterBase::HandleWeaponThrow(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->TryActivateAbilityByTag(CombatGameplayTags::Ability_WeaponThrow);
	}
}

void ACombatCharacterBase::HandleProjectileShot(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->TryActivateAbilityByTag(CombatGameplayTags::Ability_ProjectileShot);
	}
}

void ACombatCharacterBase::HandleMove(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();

	if (MoveInput.IsNearlyZero())
		return;

	// Get controller forward/right vectors projected onto ground plane
	const FRotator ControlRotation = GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDir, MoveInput.Y);
	AddMovementInput(RightDir, MoveInput.X);
}

void ACombatCharacterBase::HandleRunStarted(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->AddLooseGameplayTag(CombatGameplayTags::State_Movement_Running);
	}
}

void ACombatCharacterBase::HandleRunCompleted(const FInputActionValue& Value)
{
	if (AbilitySystemComp)
	{
		AbilitySystemComp->RemoveLooseGameplayTag(CombatGameplayTags::State_Movement_Running);
	}
}
