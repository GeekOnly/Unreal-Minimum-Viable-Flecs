#include "WindGrid.h"
#include "WindComponents.h"
#include "WindTypes.h"

void FWindGrid::Initialize()
{
	const int32 Total = GetTotalCells();
	Velocities.SetNumZeroed(Total);
	Turbulences.SetNumZeroed(Total);
}

void FWindGrid::Reset()
{
	FMemory::Memzero(Velocities.GetData(), Velocities.Num() * sizeof(FVector));
	FMemory::Memzero(Turbulences.GetData(), Turbulences.Num() * sizeof(float));
}

int32 FWindGrid::CellIndex(int32 X, int32 Y, int32 Z) const
{
	return X + Y * SizeX + Z * SizeX * SizeY;
}

bool FWindGrid::IsInBounds(int32 X, int32 Y, int32 Z) const
{
	return X >= 0 && X < SizeX && Y >= 0 && Y < SizeY && Z >= 0 && Z < SizeZ;
}

FIntVector FWindGrid::WorldToCell(const FVector& WorldPos) const
{
	const FVector Local = WorldPos - WorldOrigin;
	return FIntVector(
		FMath::FloorToInt(Local.X / CellSize),
		FMath::FloorToInt(Local.Y / CellSize),
		FMath::FloorToInt(Local.Z / CellSize)
	);
}

int32 FWindGrid::WorldToIndex(const FVector& WorldPos) const
{
	const FIntVector Cell = WorldToCell(WorldPos);
	if (!IsInBounds(Cell.X, Cell.Y, Cell.Z)) return -1;
	return CellIndex(Cell.X, Cell.Y, Cell.Z);
}

FVector FWindGrid::CellToWorld(int32 X, int32 Y, int32 Z) const
{
	return WorldOrigin + FVector(
		(X + 0.5f) * CellSize,
		(Y + 0.5f) * CellSize,
		(Z + 0.5f) * CellSize
	);
}

FVector FWindGrid::SampleVelocityAt(const FVector& WorldPos) const
{
	const FVector Local = (WorldPos - WorldOrigin) / CellSize - FVector(0.5f);
	const int32 X0 = FMath::FloorToInt(Local.X);
	const int32 Y0 = FMath::FloorToInt(Local.Y);
	const int32 Z0 = FMath::FloorToInt(Local.Z);

	const float Fx = Local.X - X0;
	const float Fy = Local.Y - Y0;
	const float Fz = Local.Z - Z0;

	FVector Result = FVector::ZeroVector;
	for (int32 Dz = 0; Dz <= 1; Dz++)
	{
		for (int32 Dy = 0; Dy <= 1; Dy++)
		{
			for (int32 Dx = 0; Dx <= 1; Dx++)
			{
				const int32 Cx = FMath::Clamp(X0 + Dx, 0, SizeX - 1);
				const int32 Cy = FMath::Clamp(Y0 + Dy, 0, SizeY - 1);
				const int32 Cz = FMath::Clamp(Z0 + Dz, 0, SizeZ - 1);

				const float Wx = Dx ? Fx : (1.f - Fx);
				const float Wy = Dy ? Fy : (1.f - Fy);
				const float Wz = Dz ? Fz : (1.f - Fz);

				Result += Velocities[CellIndex(Cx, Cy, Cz)] * Wx * Wy * Wz;
			}
		}
	}
	return Result;
}

float FWindGrid::SampleTurbulenceAt(const FVector& WorldPos) const
{
	const int32 Idx = WorldToIndex(WorldPos);
	if (Idx < 0) return 0.f;
	return Turbulences[Idx];
}

void FWindGrid::InjectMotor(const FWindMotorData& Motor, float DeltaTime)
{
	if (!Motor.bEnabled) return;

	const float RadiusSq = Motor.Radius * Motor.Radius;

	// Find bounding box of motor in grid space
	const FVector MotorMin = Motor.WorldPosition - FVector(Motor.Radius);
	const FVector MotorMax = Motor.WorldPosition + FVector(Motor.Radius);

	const FIntVector CellMin = WorldToCell(MotorMin);
	const FIntVector CellMax = WorldToCell(MotorMax);

	const int32 MinX = FMath::Clamp(CellMin.X, 0, SizeX - 1);
	const int32 MinY = FMath::Clamp(CellMin.Y, 0, SizeY - 1);
	const int32 MinZ = FMath::Clamp(CellMin.Z, 0, SizeZ - 1);
	const int32 MaxX = FMath::Clamp(CellMax.X, 0, SizeX - 1);
	const int32 MaxY = FMath::Clamp(CellMax.Y, 0, SizeY - 1);
	const int32 MaxZ = FMath::Clamp(CellMax.Z, 0, SizeZ - 1);

	for (int32 Z = MinZ; Z <= MaxZ; Z++)
	{
		for (int32 Y = MinY; Y <= MaxY; Y++)
		{
			for (int32 X = MinX; X <= MaxX; X++)
			{
				const FVector CellCenter = CellToWorld(X, Y, Z);
				const FVector ToCell = CellCenter - Motor.WorldPosition;
				const float DistSq = ToCell.SizeSquared();

				if (DistSq > RadiusSq) continue;

				const float Dist = FMath::Sqrt(DistSq);
				// Smooth falloff: 1 at center, 0 at radius
				const float Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - Dist / Motor.Radius);

				FVector Force = FVector::ZeroVector;
				const EWindEmissionType Emission = static_cast<EWindEmissionType>(Motor.EmissionType);

				switch (Emission)
				{
				case EWindEmissionType::Directional:
					Force = Motor.Direction * Motor.Strength * Falloff;
					break;
				case EWindEmissionType::Omni:
					Force = ToCell.GetSafeNormal() * Motor.Strength * Falloff;
                    // UE_LOG(LogTemp, Warning, TEXT("Omni Force: %s"), *Force.ToString());
					break;
				case EWindEmissionType::Vortex:
					{
						const FVector CrossDir = FVector::CrossProduct(Motor.Direction, ToCell.GetSafeNormal());
						Force = CrossDir * Motor.Strength * Motor.VortexAngularSpeed * Falloff;
                        // UE_LOG(LogTemp, Warning, TEXT("Vortex Force: %s"), *Force.ToString());
					}
					break;
				}

				const int32 Idx = CellIndex(X, Y, Z);
				Velocities[Idx] += Force * DeltaTime;

				// Clamp velocity magnitude
				const float MaxSpeed = Motor.Strength * 2.f;
				if (Velocities[Idx].SizeSquared() > MaxSpeed * MaxSpeed)
				{
					Velocities[Idx] = Velocities[Idx].GetSafeNormal() * MaxSpeed;
				}

				// Add turbulence near edges
				Turbulences[Idx] = FMath::Clamp(Turbulences[Idx] + Falloff * 0.1f * DeltaTime, 0.f, 1.f);
			}
		}
	}
}

void FWindGrid::DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime)
{
	const float Alpha = FMath::Clamp(DecayRate * DeltaTime, 0.f, 1.f);
	const float TurbDecay = FMath::Clamp(DecayRate * 0.5f * DeltaTime, 0.f, 1.f);

	for (int32 I = 0; I < Velocities.Num(); I++)
	{
		Velocities[I] = FMath::Lerp(Velocities[I], AmbientWind, Alpha);
		Turbulences[I] = FMath::Lerp(Turbulences[I], 0.f, TurbDecay);
	}
}
