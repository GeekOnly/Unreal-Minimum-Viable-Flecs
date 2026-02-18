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
			WindVolumeComp->MoveLength
		);
	}
}

UWindSubsystem* AWindMotorActor::GetWindSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UWindSubsystem>() : nullptr;
}
