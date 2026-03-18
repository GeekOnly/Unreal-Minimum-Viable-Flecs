#include "Animation/CombatAnimInstance.h"
#include "Core/CombatGameplayTags.h"
#include "GAS/CombatAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UCombatAnimInstance::UCombatAnimInstance()
	: Speed(0.0f)
	, bIsInAir(false)
	, bIsRunning(false)
	, bIsBoosting(false)
	, bIsAttacking(false)
	, bIsCountering(false)
	, bIsStunned(false)
	, bIsDead(false)
	, ComboCount(0)
	, ChargeAmount(0.0f)
{
}

void UCombatAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* OwningPawn = TryGetPawnOwner();
	if (ACharacter* Character = Cast<ACharacter>(OwningPawn))
	{
		CachedCharacter = Character;
	}

	if (OwningPawn)
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningPawn);
		if (ASC)
		{
			CachedASC = ASC;
		}
	}
}

void UCombatAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	ACharacter* Character = CachedCharacter.Get();
	if (!Character)
	{
		return;
	}

	// --- Movement data from CharacterMovementComponent ---
	if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
	{
		const FVector Velocity = MovementComp->Velocity;
		Speed = Velocity.Size2D();
		bIsInAir = MovementComp->IsFalling();
	}

	// --- GAS tag-based state ---
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		// Try to re-acquire ASC (may not have been ready at init)
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		if (ASC)
		{
			CachedASC = ASC;
		}
		else
		{
			return;
		}
	}

	bIsAttacking  = ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Attacking);
	bIsCountering = ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Countering);
	bIsStunned    = ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Stunned);
	bIsDead       = ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Combat_Dead);
	bIsRunning    = ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Movement_Running);
	bIsBoosting   = ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Movement_Boosting);

	// Read combo count from attribute set
	if (const UCombatAttributeSet* AttributeSet = ASC->GetSet<UCombatAttributeSet>())
	{
		ComboCount = FMath::TruncToInt32(AttributeSet->GetComboCount());
	}

	// Charge amount: read from Stamina or a custom attribute as proxy
	// (Projectile charging state uses a normalized 0-1 value)
	if (ASC->HasMatchingGameplayTag(CombatGameplayTags::State_Projectile_Charging))
	{
		// ChargeAmount accumulates; the ability is responsible for setting the actual value
		// Here we just expose whether charging is active
		ChargeAmount = FMath::Min(ChargeAmount + DeltaSeconds, 1.0f);
	}
	else
	{
		ChargeAmount = 0.0f;
	}
}

UAbilitySystemComponent* UCombatAnimInstance::GetCombatASC() const
{
	return CachedASC.Get();
}

bool UCombatAnimInstance::IsTagActive(FGameplayTag Tag) const
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (ASC && Tag.IsValid())
	{
		return ASC->HasMatchingGameplayTag(Tag);
	}
	return false;
}
