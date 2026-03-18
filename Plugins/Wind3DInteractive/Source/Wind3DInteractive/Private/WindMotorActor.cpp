#include "WindMotorActor.h"
#include "WindVolumeComponent.h"
#include "Wind3DInteractiveModule.h"
#include "Components/ArrowComponent.h"

AWindMotorActor::AWindMotorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = RootComp;

	WindVolumeComp = CreateDefaultSubobject<UWindVolumeComponent>(TEXT("WindVolume"));

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(RootComponent);
	DirectionArrow->SetArrowColor(FLinearColor(0.f, 0.8f, 1.f));
	DirectionArrow->ArrowLength = 150.f;
	DirectionArrow->ArrowSize = 1.5f;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AWindMotorActor::BeginPlay()
{
	Super::BeginPlay();

	if (!WindVolumeComp) return;

	UWindSubsystem* WindSys = GetWindSubsystem();
	if (WindSys)
	{
		ECSHandle = WindSys->RegisterWindMotor(
			GetActorLocation(),
			GetActorForwardVector(),
			WindVolumeComp->Strength,
			WindVolumeComp->Radius,
			WindVolumeComp->Shape,
			WindVolumeComp->EmissionType
		);
		UE_LOG(LogWind3D, Log, TEXT("Wind Motor registered: %s (Emission=%d)"),
			*GetName(), static_cast<int32>(WindVolumeComp->EmissionType));
	}
}

void AWindMotorActor::EndPlay(const EEndPlayReason::Type Reason)
{
	UWindSubsystem* WindSys = GetWindSubsystem();
	if (WindSys && ECSHandle.IsValid())
	{
		WindSys->UnregisterWindMotor(ECSHandle);
	}
	Super::EndPlay(Reason);
}

void AWindMotorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!WindVolumeComp) return;

	// Evaluate lifetime and curves
	float EffectiveStrength = WindVolumeComp->Strength;
	float EffectiveRadius = WindVolumeComp->Radius;
	bool bEffectiveEnabled = WindVolumeComp->bEnabled;

	if (WindVolumeComp->bUseLifetime)
	{
		ElapsedTime += DeltaTime;

		if (ElapsedTime >= WindVolumeComp->LifeTime)
		{
			if (WindVolumeComp->bLoop)
			{
				ElapsedTime = FMath::Fmod(ElapsedTime, WindVolumeComp->LifeTime);
			}
			else
			{
				bEffectiveEnabled = false;
			}
		}

		const float TimePercent = FMath::Clamp(ElapsedTime / WindVolumeComp->LifeTime, 0.f, 1.f);

		// Evaluate force curve (defaults to 1.0 if no keys)
		if (const FRichCurve* ForceCurve = WindVolumeComp->ForceCurve.GetRichCurveConst())
		{
			if (ForceCurve->GetNumKeys() > 0)
			{
				EffectiveStrength *= ForceCurve->Eval(TimePercent);
			}
		}

		// Evaluate radius curve
		if (const FRichCurve* RadiusCurve = WindVolumeComp->RadiusCurve.GetRichCurveConst())
		{
			if (RadiusCurve->GetNumKeys() > 0)
			{
				EffectiveRadius *= RadiusCurve->Eval(TimePercent);
			}
		}
	}

	// --- Compute Angular Velocity ---
	FVector AngularVelocityVec = FVector::ZeroVector;
	const FQuat CurrentRotation = GetActorQuat();

	if (WindVolumeComp->EmissionType == EWindEmissionType::Moving)
	{
		if (bRotationInitialized && DeltaTime > SMALL_NUMBER)
		{
			// Compute rotation delta: Q_delta = Q_current * Q_prev^-1
			const FQuat DeltaQuat = CurrentRotation * PreviousRotation.Inverse();

			FVector Axis;
			float AngleRad;
			DeltaQuat.ToAxisAndAngle(Axis, AngleRad);

			// Normalize angle to [-PI, PI] (handles arbitrary magnitude)
			AngleRad = FMath::Fmod(AngleRad + PI, 2.f * PI) - PI;

			// Angular velocity = axis * angle / dt (rad/s)
			if (FMath::Abs(AngleRad) > SMALL_NUMBER)
			{
				AngularVelocityVec = Axis * AngleRad / DeltaTime;
			}
		}

		// If AngularSpeed is set on the component, use it as a minimum
		// (for continuously spinning objects where per-frame delta may be small)
		if (WindVolumeComp->AngularSpeed > SMALL_NUMBER)
		{
			const FVector DirectionAngVel = GetActorForwardVector() * WindVolumeComp->AngularSpeed;
			if (AngularVelocityVec.SizeSquared() < DirectionAngVel.SizeSquared())
			{
				AngularVelocityVec = DirectionAngVel;
			}
		}

		PreviousRotation = CurrentRotation;
		bRotationInitialized = true;
	}

	UWindSubsystem* WindSys = GetWindSubsystem();
	if (WindSys && ECSHandle.IsValid())
	{
		WindSys->UpdateWindMotor(
			ECSHandle,
			GetActorLocation(),
			GetActorForwardVector(),
			EffectiveStrength,
			EffectiveRadius,
			WindVolumeComp->Shape,
			WindVolumeComp->EmissionType,
			WindVolumeComp->Height,
			0.f,
			WindVolumeComp->VortexAngularSpeed,
			bEffectiveEnabled,
			WindVolumeComp->TopRadius,
			WindVolumeComp->MoveLength,
			WindVolumeComp->ImpulseScale,
			AngularVelocityVec
		);
	}

	// --- Update DirectionArrow to show effective wind direction ---
	if (DirectionArrow)
	{
		FVector EffectiveDir = GetActorForwardVector();

		switch (WindVolumeComp->EmissionType)
		{
		case EWindEmissionType::Directional:
		case EWindEmissionType::Omni:
			// Arrow follows actor forward (default behavior)
			break;

		case EWindEmissionType::Vortex:
			{
				// Show tangential direction: cross of axis with a reference radial
				const FVector Axis = GetActorForwardVector();
				const FVector Ref = FMath::Abs(FVector::DotProduct(Axis, FVector::UpVector)) < 0.9f
					? FVector::UpVector : FVector::RightVector;
				const FVector Radial = FVector::CrossProduct(Axis, Ref).GetSafeNormal();
				EffectiveDir = FVector::CrossProduct(Axis, Radial).GetSafeNormal();
			}
			break;

		case EWindEmissionType::Moving:
			{
				// Show movement direction or rotation axis
				if (AngularVelocityVec.SizeSquared() > SMALL_NUMBER)
				{
					// Tangential: perpendicular to angular velocity axis
					const FVector AngAxis = AngularVelocityVec.GetSafeNormal();
					const FVector Ref = FMath::Abs(FVector::DotProduct(AngAxis, FVector::UpVector)) < 0.9f
						? FVector::UpVector : FVector::RightVector;
					EffectiveDir = FVector::CrossProduct(AngAxis, Ref).GetSafeNormal();
				}
				// else: keep actor forward (for linear motion, arrow already follows actor heading)
			}
			break;
		}

		DirectionArrow->SetWorldRotation(EffectiveDir.Rotation());
	}
}

UWindSubsystem* AWindMotorActor::GetWindSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UWindSubsystem>() : nullptr;
}
