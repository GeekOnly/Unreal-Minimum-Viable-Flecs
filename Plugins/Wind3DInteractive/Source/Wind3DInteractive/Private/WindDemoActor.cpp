#include "WindDemoActor.h"
#include "WindMotorActor.h"
#include "WindVolumeComponent.h"
#include "Wind3DInteractiveModule.h"

AWindDemoActor::AWindDemoActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
}

void AWindDemoActor::BeginPlay()
{
	Super::BeginPlay();
	// Key bindings: add via Blueprint (bind 1-5 keys to SetDemoMode, or N to NextMode)
	SetDemoMode(StartMode);
}

void AWindDemoActor::EndPlay(const EEndPlayReason::Type Reason)
{
	DestroyAllMotors();
	Super::EndPlay(Reason);
}

void AWindDemoActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Fan wind is driven by AngularSpeed on WindVolumeComponent (set at spawn).
	// WindMotorActor::Tick converts AngularSpeed → AngularVelocity → tangential wind.
	// No per-frame rotation needed here.
}

void AWindDemoActor::SetDemoMode(EWindDemoMode NewMode)
{
	DestroyAllMotors();
	CurrentMode = NewMode;
	SpawnMotorsForMode(NewMode);

	UE_LOG(LogWind3D, Log, TEXT("WindDemo: switched to mode %d"), (int32)NewMode);
}

void AWindDemoActor::NextMode()
{
	const int32 Next = ((int32)CurrentMode + 1) % 5;
	SetDemoMode((EWindDemoMode)Next);
}

// ---------------------------------------------------------------------------

AWindMotorActor* AWindDemoActor::SpawnMotor(FVector RelativeOffset, FRotator RelativeRotation)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector WorldPos  = GetActorLocation() + GetActorQuat().RotateVector(RelativeOffset);
	const FRotator WorldRot = (GetActorRotation().Quaternion() * RelativeRotation.Quaternion()).Rotator();

	AWindMotorActor* Motor = GetWorld()->SpawnActor<AWindMotorActor>(
		AWindMotorActor::StaticClass(), WorldPos, WorldRot, Params);

	if (Motor)
	{
		// Attach so the motor follows DemoActor when moved at runtime
		Motor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		ActiveMotors.Add(Motor);
	}
	return Motor;
}

void AWindDemoActor::SpawnMotorsForMode(EWindDemoMode Mode)
{
	FanAngles.Reset();

	switch (Mode)
	{
	// -----------------------------------------------------------------------
	// 1. Fan Turbine — Cylinder + Moving (tangential) + slow wave pulse
	// -----------------------------------------------------------------------
	case EWindDemoMode::FanTurbine:
	{
		AWindMotorActor* M = SpawnMotor(FVector::ZeroVector, FRotator::ZeroRotator);
		if (M && M->GetWindVolume())
		{
			UWindVolumeComponent* V = M->GetWindVolume();
			V->Shape         = EWindMotorShape::Cylinder;
			V->EmissionType  = EWindEmissionType::Moving;
			V->Radius        = MotorRadius;
			V->Height        = 80.f;
			V->Strength      = MotorStrength;
			// AngularSpeed drives tangential wind (ω × r). FanRPM → rad/s
			V->AngularSpeed  = FanRPM / 60.f * 2.f * PI;
			// Slow wave: 1 big pulse every ~2s — travels across grid before next pulse
			V->bWaveMode     = true;
			V->WaveFrequency = 0.5f;
			V->WaveMin       = 0.0f;  // full silence at trough = maximum contrast
		}
		break;
	}

	// -----------------------------------------------------------------------
	// 2. Tornado — Cylinder + Vortex
	// -----------------------------------------------------------------------
	case EWindDemoMode::Tornado:
	{
		AWindMotorActor* M = SpawnMotor(FVector::ZeroVector, FRotator(0.f, 0.f, 0.f));
		if (M && M->GetWindVolume())
		{
			UWindVolumeComponent* V = M->GetWindVolume();
			V->Shape              = EWindMotorShape::Cylinder;
			V->EmissionType       = EWindEmissionType::Vortex;
			V->Radius             = MotorRadius;
			V->Height             = 800.f;
			V->Strength           = MotorStrength * 0.8f;
			V->VortexAngularSpeed = 2.5f;
		}
		break;
	}

	// -----------------------------------------------------------------------
	// 3. Explosion — Sphere + Omni + single-shot Lifetime pulse
	// -----------------------------------------------------------------------
	case EWindDemoMode::Explosion:
	{
		AWindMotorActor* M = SpawnMotor(FVector::ZeroVector, FRotator::ZeroRotator);
		if (M && M->GetWindVolume())
		{
			UWindVolumeComponent* V = M->GetWindVolume();
			V->Shape        = EWindMotorShape::Sphere;
			V->EmissionType = EWindEmissionType::Omni;
			V->Radius       = MotorRadius * 1.5f;
			V->Strength     = MotorStrength * 2.f;
			V->ImpulseScale = 3.f;
			// One-shot: ramp up then off in 1 second
			V->bUseLifetime = true;
			V->bLoop        = true;
			V->LifeTime     = 2.f;
			// ForceCurve: 0→peak at 0.15→0 by 0.5 → silence rest of cycle
			FRichCurve* C = V->ForceCurve.GetRichCurve();
			C->AddKey(0.00f, 0.0f);
			C->AddKey(0.15f, 1.0f);
			C->AddKey(0.50f, 0.0f);
			C->AddKey(1.00f, 0.0f);
		}
		break;
	}

	// -----------------------------------------------------------------------
	// 4. Directional Gust — Sphere + Directional + Lifetime gust curve
	// -----------------------------------------------------------------------
	case EWindDemoMode::DirectGust:
	{
		AWindMotorActor* M = SpawnMotor(FVector::ZeroVector, FRotator::ZeroRotator);
		if (M && M->GetWindVolume())
		{
			UWindVolumeComponent* V = M->GetWindVolume();
			V->Shape        = EWindMotorShape::Sphere;
			V->EmissionType = EWindEmissionType::Directional;
			V->Radius       = MotorRadius * 1.2f;
			V->Strength     = MotorStrength;
			V->bUseLifetime = true;
			V->bLoop        = true;
			V->LifeTime     = 3.f;
			// ForceCurve: slow build → strong peak → slow decay → pause
			FRichCurve* C = V->ForceCurve.GetRichCurve();
			C->AddKey(0.00f, 0.05f);
			C->AddKey(0.30f, 1.00f);
			C->AddKey(0.55f, 0.70f);
			C->AddKey(0.80f, 0.10f);
			C->AddKey(1.00f, 0.05f);
		}
		break;
	}

	// -----------------------------------------------------------------------
	// 5. Multi-Fan Array — N fans side-by-side, staggered wave phase
	// -----------------------------------------------------------------------
	case EWindDemoMode::MultiFan:
	{
		const float HalfSpan = FanSpacing * (FanCount - 1) * 0.5f;
		for (int32 i = 0; i < FanCount; ++i)
		{
			const float Offset = i * FanSpacing - HalfSpan;
			AWindMotorActor* M = SpawnMotor(FVector(0.f, Offset, 0.f), FRotator::ZeroRotator);
			if (M && M->GetWindVolume())
			{
				UWindVolumeComponent* V = M->GetWindVolume();
				V->Shape         = EWindMotorShape::Cylinder;
				V->EmissionType  = EWindEmissionType::Directional;
				V->Radius        = MotorRadius;
				V->Height        = 80.f;
				V->Strength      = MotorStrength;
				V->bWaveMode     = true;
				V->WaveFrequency = 5.f;
				V->WaveMin       = 0.1f;
				// Stagger phase so fans create cascading wave
				V->WavePhaseOffset = i * (1.f / FanCount) / 5.f; // 1/N cycle apart
			}
		}
		break;
	}
	}
}

void AWindDemoActor::DestroyAllMotors()
{
	for (AWindMotorActor* M : ActiveMotors)
	{
		if (M) M->Destroy();
	}
	ActiveMotors.Empty();
	FanAngles.Reset();
}
