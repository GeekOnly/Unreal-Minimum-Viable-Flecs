#include "GAS/Abilities/GA_ProjectileShot.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "Components/TargetingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UGA_ProjectileShot::UGA_ProjectileShot()
{
	AbilityTags.AddTag(CombatGameplayTags::Ability_ProjectileShot);
	ActivationOwnedTags.AddTag(CombatGameplayTags::State_Projectile_Charging);

	// Block while already charging or in other combat actions
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Projectile_Charging);
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Attacking);
}

void UGA_ProjectileShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = GetCombatCharacter();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Cache targeting component
	CachedTargetingComp = Character->FindComponentByClass<UTargetingComponent>();

	StartCharge();
}

void UGA_ProjectileShot::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const ACharacter* Character = GetCombatCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(ChargeTickHandle);
		Character->GetWorldTimerManager().ClearTimer(SlowMotionRestoreHandle);
	}

	// Ensure time dilation is restored
	RestoreTimeDilation();

	CurrentCharge = 0.0f;
	ChargeStartTime = 0.0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ProjectileShot::StartCharge()
{
	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	CurrentCharge = 0.0f;
	ChargeStartTime = FPlatformTime::Seconds();

	// Play charge montage
	UAnimMontage* Montage = ChargeMontage.LoadSynchronous();
	if (Montage)
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(Montage);
		}
	}

	// Start charge tick
	constexpr float TickRate = 1.0f / 60.0f;
	Character->GetWorldTimerManager().SetTimer(
		ChargeTickHandle,
		FTimerDelegate::CreateUObject(this, &UGA_ProjectileShot::TickCharge),
		TickRate,
		true
	);

	// Wait for input release via AbilityTask
	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	if (WaitRelease)
	{
		WaitRelease->OnRelease.AddDynamic(this, &UGA_ProjectileShot::OnInputReleased);
		WaitRelease->ReadyForActivation();
	}
}

void UGA_ProjectileShot::TickCharge()
{
	const double CurrentTime = FPlatformTime::Seconds();
	const float Elapsed = static_cast<float>(CurrentTime - ChargeStartTime);

	// Normalize charge to [0, 1]
	CurrentCharge = FMath::Clamp(Elapsed / ChargeDuration, 0.0f, 1.0f);
}

void UGA_ProjectileShot::OnInputReleased(float TimeHeld)
{
	// Stop charging
	if (const ACharacter* Character = GetCombatCharacter())
	{
		Character->GetWorldTimerManager().ClearTimer(ChargeTickHandle);
	}

	ReleaseShot();
}

void UGA_ProjectileShot::ReleaseShot()
{
	ACharacter* Character = GetCombatCharacter();
	if (!Character)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Play fire montage
	UAnimMontage* Montage = FireMontage.LoadSynchronous();
	if (Montage)
	{
		UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			// Stop charge montage and play fire montage
			AnimInstance->StopAllMontages(0.1f);
			AnimInstance->Montage_Play(Montage);

			// Bind end delegate to end ability
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &UCombatAbilityBase::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
		}
	}

	// Evaluate charge quality
	const EChargeQuality Quality = EvaluateChargeQuality(CurrentCharge);

	// Find target
	AActor* Target = nullptr;
	if (CachedTargetingComp.IsValid())
	{
		Target = CachedTargetingComp->GetCurrentTarget();
	}

	// Determine spawn origin and direction
	FVector SpawnOrigin = Character->GetActorLocation();
	if (Character->GetMesh())
	{
		const FTransform MuzzleTransform = Character->GetMesh()->GetSocketTransform(MuzzleSocketName);
		SpawnOrigin = MuzzleTransform.GetLocation();
	}

	FVector AimDirection = Character->GetActorForwardVector();
	if (Target)
	{
		AimDirection = (Target->GetActorLocation() - SpawnOrigin).GetSafeNormal();
	}

	UCombatAbilitySystemComponent* ASC = GetCombatASC();

	switch (Quality)
	{
	case EChargeQuality::Full:
		{
			SpawnProjectile(SpawnOrigin, AimDirection);

			// Send hit event
			if (ASC)
			{
				FGameplayEventData EventData;
				EventData.Instigator = Character;
				EventData.Target = Target;
				ASC->HandleGameplayEvent(CombatGameplayTags::Event_Projectile_Hit, &EventData);
			}

			// Activate boost
			if (ASC)
			{
				FGameplayEventData BoostEvent;
				BoostEvent.Instigator = Character;
				ASC->HandleGameplayEvent(CombatGameplayTags::Event_Boost_Activated, &BoostEvent);
			}
			break;
		}
	case EChargeQuality::Half:
		{
			SpawnProjectile(SpawnOrigin, AimDirection);

			// Send hit event
			if (ASC)
			{
				FGameplayEventData EventData;
				EventData.Instigator = Character;
				EventData.Target = Target;
				ASC->HandleGameplayEvent(CombatGameplayTags::Event_Projectile_Hit, &EventData);
			}

			// Apply slow-motion for dramatic feedback
			ApplySlowMotion();

			// Also activate boost
			if (ASC)
			{
				FGameplayEventData BoostEvent;
				BoostEvent.Instigator = Character;
				ASC->HandleGameplayEvent(CombatGameplayTags::Event_Boost_Activated, &BoostEvent);
			}
			break;
		}
	case EChargeQuality::Miss:
		{
			// Add angular deviation for miss
			const FRotator Deviation(
				FMath::RandRange(-MissDeviationDegrees, MissDeviationDegrees),
				FMath::RandRange(-MissDeviationDegrees, MissDeviationDegrees),
				0.0f
			);
			const FVector MissDirection = Deviation.RotateVector(AimDirection);
			SpawnProjectile(SpawnOrigin, MissDirection);
			break;
		}
	}

	// Ability ends when fire montage ends (via OnMontageEnded)
	// If no fire montage, end now
	if (!Montage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

EChargeQuality UGA_ProjectileShot::EvaluateChargeQuality(float NormalizedCharge) const
{
	if (NormalizedCharge >= 0.9f)
	{
		return EChargeQuality::Full;
	}

	if (FMath::Abs(NormalizedCharge - 0.5f) <= HalfChargePrecision)
	{
		return EChargeQuality::Half;
	}

	return EChargeQuality::Miss;
}

AActor* UGA_ProjectileShot::SpawnProjectile(const FVector& Origin, const FVector& Direction)
{
	if (!ProjectileClass)
		return nullptr;

	UWorld* World = GetWorld();
	if (!World)
		return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetCombatCharacter();
	SpawnParams.Instigator = GetCombatCharacter();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FRotator SpawnRotation = Direction.Rotation();
	AActor* Projectile = World->SpawnActor<AActor>(ProjectileClass, Origin, SpawnRotation, SpawnParams);

	if (Projectile)
	{
		// If the projectile has a ProjectileMovementComponent, set its velocity
		UProjectileMovementComponent* ProjMovement = Projectile->FindComponentByClass<UProjectileMovementComponent>();
		if (ProjMovement)
		{
			ProjMovement->Velocity = Direction * ProjectileSpeed;
			ProjMovement->InitialSpeed = ProjectileSpeed;
			ProjMovement->MaxSpeed = ProjectileSpeed;
		}
	}

	return Projectile;
}

void UGA_ProjectileShot::ApplySlowMotion()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	World->GetWorldSettings()->SetTimeDilation(SlowMotionScale);

	// Schedule restoration (timer uses real time, so account for dilation)
	const float RealDuration = SlowMotionDuration;
	World->GetTimerManager().SetTimer(
		SlowMotionRestoreHandle,
		FTimerDelegate::CreateUObject(this, &UGA_ProjectileShot::RestoreTimeDilation),
		RealDuration,
		false
	);
}

void UGA_ProjectileShot::RestoreTimeDilation()
{
	UWorld* World = GetWorld();
	if (World && World->GetWorldSettings())
	{
		World->GetWorldSettings()->SetTimeDilation(1.0f);
	}
}
