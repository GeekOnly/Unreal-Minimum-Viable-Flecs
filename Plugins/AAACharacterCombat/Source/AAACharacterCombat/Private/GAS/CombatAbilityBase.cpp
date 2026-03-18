#include "GAS/CombatAbilityBase.h"
#include "GAS/CombatAbilitySystemComponent.h"
#include "GAS/CombatAttributeSet.h"
#include "Data/CombatAttackData.h"
#include "Components/CombatHitStopComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCueManager.h"

UCombatAbilityBase::UCombatAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Default: block activation while any combat state is active
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Dead);
	ActivationBlockedTags.AddTag(CombatGameplayTags::State_Combat_Stunned);
}

UCombatAbilitySystemComponent* UCombatAbilityBase::GetCombatASC() const
{
	return Cast<UCombatAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

ACharacter* UCombatAbilityBase::GetCombatCharacter() const
{
	return Cast<ACharacter>(GetAvatarActorFromActorInfo());
}

void UCombatAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Add activation owned tags
	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (ASC && ActivationOwnedTags.Num() > 0)
	{
		ASC->AddLooseGameplayTags(ActivationOwnedTags);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UCombatAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Remove activation owned tags
	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (ASC && ActivationOwnedTags.Num() > 0)
	{
		ASC->RemoveLooseGameplayTags(ActivationOwnedTags);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCombatAbilityBase::PlayAttackMontage(const UCombatAttackData* AttackData, AActor* Target)
{
	if (!AttackData)
		return;

	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	// Setup motion warping before playing montage
	if (AttackData->bUseMotionWarping && Target)
	{
		float Distance = FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation());
		if (Distance <= AttackData->MaxWarpDistance)
		{
			FVector WarpLocation = Target->GetActorLocation() -
				(Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal() * AttackData->WarpStopOffset;
			FRotator WarpRotation = (Target->GetActorLocation() - Character->GetActorLocation()).Rotation();
			SetMotionWarpTarget(AttackData->WarpTargetName, WarpLocation, WarpRotation);
		}
	}

	// Load and play montage
	UAnimMontage* Montage = AttackData->AttackMontage.LoadSynchronous();
	if (!Montage)
		return;

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
		return;

	AnimInstance->Montage_Play(Montage, AttackData->PlayRate);

	if (AttackData->MontageSectionName != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(AttackData->MontageSectionName, Montage);
	}

	// Bind montage end
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UCombatAbilityBase::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
}

void UCombatAbilityBase::ApplyDamageToTarget(AActor* Target, const UCombatAttackData* AttackData)
{
	if (!Target || !AttackData || !AttackData->DamageEffect)
		return;

	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (!ASC)
		return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
		return;

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(AttackData->DamageEffect, 1.0f, Context);
	if (Spec.IsValid())
	{
		// Set damage via SetByCaller using the Effect.Damage tag
		Spec.Data->SetSetByCallerMagnitude(CombatGameplayTags::Effect_Damage, AttackData->BaseDamage);
		ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

void UCombatAbilityBase::ApplyKnockback(AActor* Target, float Force, FVector Direction)
{
	if (!Target || Direction.IsNearlyZero())
		return;

	ACharacter* TargetChar = Cast<ACharacter>(Target);
	if (TargetChar && TargetChar->GetCharacterMovement())
	{
		TargetChar->GetCharacterMovement()->AddImpulse(Direction.GetSafeNormal() * Force, true);
	}
}

void UCombatAbilityBase::SetMotionWarpTarget(FName WarpName, FVector Location, FRotator Rotation)
{
	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	UMotionWarpingComponent* MotionWarp = Character->FindComponentByClass<UMotionWarpingComponent>();
	if (MotionWarp)
	{
		FMotionWarpingTarget WarpTarget;
		WarpTarget.Name = WarpName;
		WarpTarget.Location = Location;
		WarpTarget.Rotation = FQuat(Rotation);
		MotionWarp->AddOrUpdateWarpTarget(WarpTarget);
	}
}

void UCombatAbilityBase::ClearMotionWarpTarget(FName WarpName)
{
	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	UMotionWarpingComponent* MotionWarp = Character->FindComponentByClass<UMotionWarpingComponent>();
	if (MotionWarp)
	{
		MotionWarp->RemoveWarpTarget(WarpName);
	}
}

void UCombatAbilityBase::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
	}
}

void UCombatAbilityBase::ApplyHitStop(float Duration)
{
	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	UCombatHitStopComponent* HitStopComp = Character->FindComponentByClass<UCombatHitStopComponent>();
	if (HitStopComp)
	{
		if (Duration > 0.0f)
		{
			HitStopComp->ApplyHitStop(Duration, HitStopComp->DefaultTimeDilation);
		}
		else
		{
			HitStopComp->ApplyHitStop(HitStopComp->DefaultHitStopDuration, HitStopComp->DefaultTimeDilation);
		}
	}
}

void UCombatAbilityBase::PlayCombatCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	if (!ShakeClass)
		return;

	ACharacter* Character = GetCombatCharacter();
	if (!Character)
		return;

	UWorld* World = Character->GetWorld();
	if (!World)
		return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		PC->ClientStartCameraShake(ShakeClass, Scale);
	}
}

void UCombatAbilityBase::ExecuteHitResponse(AActor* Target, const UCombatAttackData* AttackData, const FHitResult& HitResult)
{
	if (!Target || !AttackData)
		return;

	// 1. Hit stop
	ApplyHitStop();

	// 2. Camera shake (scale from attack data)
	// Camera shake class should be set on the attack data or use a project default
	// For now we trigger a GameplayCue which handles VFX/SFX/CameraShake

	// 3. Trigger GameplayCue for VFX/SFX
	UCombatAbilitySystemComponent* ASC = GetCombatASC();
	if (ASC)
	{
		FGameplayCueParameters CueParams;
		CueParams.EffectContext = ASC->MakeEffectContext();
		CueParams.EffectContext.AddHitResult(HitResult);
		CueParams.RawMagnitude = AttackData->BaseDamage;
		CueParams.NormalizedMagnitude = AttackData->CameraShakeScale;

		ASC->ExecuteGameplayCue(CombatGameplayTags::GameplayCue_Combat_HitImpact, CueParams);
	}

	// 4. Apply damage
	ApplyDamageToTarget(Target, AttackData);

	// 5. Apply knockback
	ACharacter* Character = GetCombatCharacter();
	if (Character && AttackData->KnockbackForce > 0.0f)
	{
		FVector KnockDir = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal();
		if (AttackData->LaunchAngle > 0.0f)
		{
			KnockDir = KnockDir.RotateAngleAxis(-AttackData->LaunchAngle, FVector::RightVector);
		}
		ApplyKnockback(Target, AttackData->KnockbackForce, KnockDir);
	}
}
