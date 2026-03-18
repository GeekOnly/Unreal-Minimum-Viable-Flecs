#include "Characters/EnemyCombatCharacter.h"
#include "Core/CombatGameplayTags.h"
#include "Core/CombatTypes.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "AI/CombatAIManagerSubsystem.h"
#include "Components/TargetingComponent.h"
#include "MotionWarpingComponent.h"
#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"

AEnemyCombatCharacter::AEnemyCombatCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create GAS component
	AbilitySystemComp = CreateDefaultSubobject<UCombatAbilitySystemComponent>(TEXT("AbilitySystemComp"));
	AbilitySystemComp->SetIsReplicated(true);
	AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Create MotionWarping
	MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));

	// Use AIController by default
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

UAbilitySystemComponent* AEnemyCombatCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void AEnemyCombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
	RegisterWithSystems();
}

void AEnemyCombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSystems();
	Super::EndPlay(EndPlayReason);
}

void AEnemyCombatCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Re-initialize ability actor info on the server
	if (AbilitySystemComp)
	{
		AbilitySystemComp->InitAbilityActorInfo(this, this);
	}
}

// -----------------------------------------------------------------------------
// GAS Initialization
// -----------------------------------------------------------------------------

void AEnemyCombatCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComp)
		return;

	AbilitySystemComp->InitAbilityActorInfo(this, this);

	// Only grant abilities on the server
	if (HasAuthority())
	{
		AbilitySystemComp->GrantDefaultAbilities(DefaultAbilities);

		for (const TSubclassOf<UGameplayEffect>& EffectClass : DefaultEffects)
		{
			if (EffectClass)
			{
				AbilitySystemComp->ApplyEffectToSelf(EffectClass);
			}
		}
	}

	// Set initial AI state
	AbilitySystemComp->AddLooseGameplayTag(CombatGameplayTags::AI_State_Idle);
}

// -----------------------------------------------------------------------------
// System Registration
// -----------------------------------------------------------------------------

void AEnemyCombatCharacter::RegisterWithSystems()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	// Register with the AI Manager Subsystem
	UCombatAIManagerSubsystem* AIManager = World->GetSubsystem<UCombatAIManagerSubsystem>();
	if (AIManager)
	{
		AIManager->RegisterEnemy(this);
	}

	// Register with the player's TargetingComponent
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		UTargetingComponent* PlayerTargeting = PlayerPawn->FindComponentByClass<UTargetingComponent>();
		if (PlayerTargeting)
		{
			PlayerTargeting->RegisterEnemy(this);
		}
	}
}

void AEnemyCombatCharacter::UnregisterFromSystems()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	// Unregister from the AI Manager Subsystem
	UCombatAIManagerSubsystem* AIManager = World->GetSubsystem<UCombatAIManagerSubsystem>();
	if (AIManager)
	{
		AIManager->UnregisterEnemy(this);
	}

	// Unregister from the player's TargetingComponent
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		UTargetingComponent* PlayerTargeting = PlayerPawn->FindComponentByClass<UTargetingComponent>();
		if (PlayerTargeting)
		{
			PlayerTargeting->UnregisterEnemy(this);
		}
	}
}
