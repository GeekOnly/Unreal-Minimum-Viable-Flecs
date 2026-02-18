#include "WindGrid.h"
#include "WindComponents.h"
#include "WindTypes.h"

void FWindGrid::Initialize()
{
	const int32 Total = GetTotalCells();
	Velocities.SetNumZeroed(Total);
	VelocitiesBack.SetNumZeroed(Total);
	Turbulences.SetNumZeroed(Total);
}

void FWindGrid::Reset()
{
	FMemory::Memzero(Velocities.GetData(), Velocities.Num() * sizeof(FVector));
	FMemory::Memzero(VelocitiesBack.GetData(), VelocitiesBack.Num() * sizeof(FVector));
	FMemory::Memzero(Turbulences.GetData(), Turbulences.Num() * sizeof(float));
}

void FWindGrid::SwapBuffers()
{
	Swap(Velocities, VelocitiesBack);
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

// ---------- GoW Simulation Pipeline ----------

FVector FWindGrid::SampleVelocityAtLocal(float Lx, float Ly, float Lz) const
{
	// Trilinear interpolation in grid-local coordinates (cell units, 0-based)
	const int32 X0 = FMath::FloorToInt(Lx);
	const int32 Y0 = FMath::FloorToInt(Ly);
	const int32 Z0 = FMath::FloorToInt(Lz);

	const float Fx = Lx - X0;
	const float Fy = Ly - Y0;
	const float Fz = Lz - Z0;

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

void FWindGrid::Diffuse(float DiffusionRate, float DeltaTime)
{
	// GoW-style 3D diffusion: 6-neighbor Laplacian blur
	const float Alpha = DiffusionRate * DeltaTime;
	if (Alpha <= 0.f) return;

	for (int32 Z = 0; Z < SizeZ; Z++)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				const int32 Idx = CellIndex(X, Y, Z);
				const FVector& Center = Velocities[Idx];

				// Sample 6 neighbors (clamp at boundaries)
				const FVector Xp = Velocities[CellIndex(FMath::Min(X + 1, SizeX - 1), Y, Z)];
				const FVector Xn = Velocities[CellIndex(FMath::Max(X - 1, 0), Y, Z)];
				const FVector Yp = Velocities[CellIndex(X, FMath::Min(Y + 1, SizeY - 1), Z)];
				const FVector Yn = Velocities[CellIndex(X, FMath::Max(Y - 1, 0), Z)];
				const FVector Zp = Velocities[CellIndex(X, Y, FMath::Min(Z + 1, SizeZ - 1))];
				const FVector Zn = Velocities[CellIndex(X, Y, FMath::Max(Z - 1, 0))];

				const FVector Laplacian = (Xp + Xn + Yp + Yn + Zp + Zn) - Center * 6.f;
				VelocitiesBack[Idx] = Center + Laplacian * Alpha;
			}
		}
	}

	SwapBuffers();
}

void FWindGrid::Advect(float AdvectionForce, float DeltaTime)
{
	// GoW-style semi-Lagrangian advection:
	// Forward pass: distribute velocity to target cells
	// Then reverse pass: gather from source cells
	// Combined here as reverse-advection (backtrace) for stability

	const float Factor = AdvectionForce * DeltaTime / CellSize;
	if (FMath::Abs(Factor) < SMALL_NUMBER) return;

	// Reverse advection: for each cell, backtrace along velocity to find source
	for (int32 Z = 0; Z < SizeZ; Z++)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				const int32 Idx = CellIndex(X, Y, Z);
				const FVector& Vel = Velocities[Idx];

				// Backtrace: where did this cell's wind come from?
				const float SrcX = (float)X - Vel.X * Factor;
				const float SrcY = (float)Y - Vel.Y * Factor;
				const float SrcZ = (float)Z - Vel.Z * Factor;

				// Trilinear sample from source position
				VelocitiesBack[Idx] = SampleVelocityAtLocal(SrcX, SrcY, SrcZ);
			}
		}
	}

	SwapBuffers();
}

void FWindGrid::ShiftData(FIntVector CellOffset, FVector AmbientWind)
{
	// GoW-style grid offset: when center moves, shift all data in opposite direction
	// so wind stays in correct world-space position
	if (CellOffset.X == 0 && CellOffset.Y == 0 && CellOffset.Z == 0) return;

	const int32 Total = GetTotalCells();

	// Fill back buffer with ambient (new cells default to ambient)
	for (int32 I = 0; I < Total; I++)
	{
		VelocitiesBack[I] = AmbientWind;
	}

	for (int32 Z = 0; Z < SizeZ; Z++)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				// Source cell in old data
				const int32 SrcX = X + CellOffset.X;
				const int32 SrcY = Y + CellOffset.Y;
				const int32 SrcZ = Z + CellOffset.Z;

				if (IsInBounds(SrcX, SrcY, SrcZ))
				{
					VelocitiesBack[CellIndex(X, Y, Z)] = Velocities[CellIndex(SrcX, SrcY, SrcZ)];
				}
			}
		}
	}

	SwapBuffers();
}
