#include "WindGridCPU.h"
#include "WindComponents.h"
#include "WindTypes.h"
#include "Async/ParallelFor.h"

// ============================ Setup ============================

void FWindGridCPU::Initialize(int32 InSizeX, int32 InSizeY, int32 InSizeZ, float InCellSize)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	SizeZ = InSizeZ;
	CellSize = InCellSize;

	const int32 Total = GetTotalCells();
	Velocities.SetNumZeroed(Total);
	VelocitiesBack.SetNumZeroed(Total);
	Turbulences.SetNumZeroed(Total);
	Solids.SetNumZeroed(Total);
	AdvectWeights.SetNumZeroed(Total);
}

void FWindGridCPU::Reset()
{
	FMemory::Memzero(Velocities.GetData(), Velocities.Num() * sizeof(FVector));
	FMemory::Memzero(VelocitiesBack.GetData(), VelocitiesBack.Num() * sizeof(FVector));
	FMemory::Memzero(Turbulences.GetData(), Turbulences.Num() * sizeof(float));
	FMemory::Memzero(Solids.GetData(), Solids.Num() * sizeof(uint8));
}

// ============================ Obstacle Grid ============================

void FWindGridCPU::ClearSolids()
{
	FMemory::Memzero(Solids.GetData(), Solids.Num() * sizeof(uint8));
}

void FWindGridCPU::MarkSolid(int32 X, int32 Y, int32 Z)
{
	if (IsInBounds(X, Y, Z))
	{
		Solids[CellIndex(X, Y, Z)] = 1;
	}
}

bool FWindGridCPU::IsSolid(int32 X, int32 Y, int32 Z) const
{
	if (!IsInBounds(X, Y, Z)) return false;
	return Solids[CellIndex(X, Y, Z)] != 0;
}

// ============================ Coordinate Helpers ============================

void FWindGridCPU::SwapBuffers()
{
	Swap(Velocities, VelocitiesBack);
}

int32 FWindGridCPU::CellIndex(int32 X, int32 Y, int32 Z) const
{
	return X + Y * SizeX + Z * SizeX * SizeY;
}

bool FWindGridCPU::IsInBounds(int32 X, int32 Y, int32 Z) const
{
	return X >= 0 && X < SizeX && Y >= 0 && Y < SizeY && Z >= 0 && Z < SizeZ;
}

FIntVector FWindGridCPU::WorldToCell(const FVector& WorldPos) const
{
	const FVector Local = WorldPos - WorldOrigin;
	return FIntVector(
		FMath::FloorToInt(Local.X / CellSize),
		FMath::FloorToInt(Local.Y / CellSize),
		FMath::FloorToInt(Local.Z / CellSize)
	);
}

int32 FWindGridCPU::WorldToIndex(const FVector& WorldPos) const
{
	const FIntVector Cell = WorldToCell(WorldPos);
	if (!IsInBounds(Cell.X, Cell.Y, Cell.Z)) return -1;
	return CellIndex(Cell.X, Cell.Y, Cell.Z);
}

FVector FWindGridCPU::CellToWorld(int32 X, int32 Y, int32 Z) const
{
	return WorldOrigin + FVector(
		(X + 0.5f) * CellSize,
		(Y + 0.5f) * CellSize,
		(Z + 0.5f) * CellSize
	);
}

// ============================ Query ============================

FVector FWindGridCPU::SampleVelocityAt(const FVector& WorldPos) const
{
	const FVector Local = (WorldPos - WorldOrigin) / CellSize - FVector(0.5f);
	if (!FMath::IsFinite(Local.X) || !FMath::IsFinite(Local.Y) || !FMath::IsFinite(Local.Z))
		return FVector::ZeroVector;
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

FVector FWindGridCPU::SampleVelocityAtLocal(float Lx, float Ly, float Lz) const
{
	if (!FMath::IsFinite(Lx) || !FMath::IsFinite(Ly) || !FMath::IsFinite(Lz))
		return FVector::ZeroVector;
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

float FWindGridCPU::SampleTurbulenceAt(const FVector& WorldPos) const
{
	const int32 Idx = WorldToIndex(WorldPos);
	if (Idx < 0) return 0.f;
	return Turbulences[Idx];
}

// ============================ Motor Injection ============================

void FWindGridCPU::InjectMotor(const FWindMotorData& Motor, float DeltaTime)
{
	if (!Motor.bEnabled) return;

	const EWindMotorShape MotorShape = static_cast<EWindMotorShape>(Motor.Shape);
	const EWindEmissionType Emission = static_cast<EWindEmissionType>(Motor.EmissionType);

	// Compute bounding box extent based on shape
	float BoundsExtent = Motor.Radius;
	if (MotorShape == EWindMotorShape::Cylinder)
	{
		const float EffTopRadius = Motor.TopRadius > 0.f ? Motor.TopRadius : Motor.Radius;
		BoundsExtent = FMath::Max3(Motor.Radius, EffTopRadius, Motor.Height * 0.5f);
	}
	if (Emission == EWindEmissionType::Moving)
	{
		BoundsExtent = FMath::Max(BoundsExtent, Motor.MoveLength + Motor.Radius);
	}

	// Bounding box in world space
	FVector BoundsMin, BoundsMax;
	if (Emission == EWindEmissionType::Moving)
	{
		BoundsMin = FVector::Min(Motor.WorldPosition, Motor.PreviousPosition) - FVector(BoundsExtent);
		BoundsMax = FVector::Max(Motor.WorldPosition, Motor.PreviousPosition) + FVector(BoundsExtent);
	}
	else if (MotorShape == EWindMotorShape::Cylinder)
	{
		const FVector HalfAxis = Motor.Direction * Motor.Height * 0.5f;
		const float MaxR = FMath::Max(Motor.Radius, Motor.TopRadius > 0.f ? Motor.TopRadius : Motor.Radius);
		BoundsMin = Motor.WorldPosition - FVector(MaxR) - HalfAxis.GetAbs();
		BoundsMax = Motor.WorldPosition + FVector(MaxR) + HalfAxis.GetAbs();
	}
	else
	{
		BoundsMin = Motor.WorldPosition - FVector(Motor.Radius);
		BoundsMax = Motor.WorldPosition + FVector(Motor.Radius);
	}

	const FIntVector CellMin = WorldToCell(BoundsMin);
	const FIntVector CellMax = WorldToCell(BoundsMax);

	const int32 MinX = FMath::Clamp(CellMin.X, 0, SizeX - 1);
	const int32 MinY = FMath::Clamp(CellMin.Y, 0, SizeY - 1);
	const int32 MinZ = FMath::Clamp(CellMin.Z, 0, SizeZ - 1);
	const int32 MaxX = FMath::Clamp(CellMax.X, 0, SizeX - 1);
	const int32 MaxY = FMath::Clamp(CellMax.Y, 0, SizeY - 1);
	const int32 MaxZ = FMath::Clamp(CellMax.Z, 0, SizeZ - 1);

	const float RadiusSq = Motor.Radius * Motor.Radius;

	for (int32 Z = MinZ; Z <= MaxZ; Z++)
	{
		for (int32 Y = MinY; Y <= MaxY; Y++)
		{
			for (int32 X = MinX; X <= MaxX; X++)
			{
				const FVector CellCenter = CellToWorld(X, Y, Z);

				// --- Shape containment test + falloff ---
				float Falloff = 0.f;

				if (Emission == EWindEmissionType::Moving)
				{
					if (MotorShape == EWindMotorShape::Cylinder)
					{
						const FVector ToCell = CellCenter - Motor.WorldPosition;
						const float AxisProj = FVector::DotProduct(ToCell, Motor.Direction);
						const float HalfHeight = Motor.Height * 0.5f;

						if (FMath::Abs(AxisProj) > HalfHeight) continue;

						const float EffTopRadius = Motor.TopRadius > 0.f ? Motor.TopRadius : Motor.Radius;
						const float HeightFrac = (AxisProj + HalfHeight) / Motor.Height;
						const float LocalRadius = FMath::Lerp(Motor.Radius, EffTopRadius, HeightFrac);

						const FVector RadialVec = ToCell - Motor.Direction * AxisProj;
						const float RadialDist = RadialVec.Size();

						if (RadialDist > LocalRadius) continue;

						Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - RadialDist / LocalRadius);
					}
					else
					{
						const FVector MoveDir = Motor.WorldPosition - Motor.PreviousPosition;
						const float MoveLenSq = MoveDir.SizeSquared();

						FVector ClosestPoint;
						if (MoveLenSq < SMALL_NUMBER)
						{
							ClosestPoint = Motor.WorldPosition;
						}
						else
						{
							const float T = FMath::Clamp(
								FVector::DotProduct(CellCenter - Motor.PreviousPosition, MoveDir) / MoveLenSq,
								0.f, 1.f);
							ClosestPoint = Motor.PreviousPosition + MoveDir * T;
						}

						const float DistSq = (CellCenter - ClosestPoint).SizeSquared();
						if (DistSq > RadiusSq) continue;

						const float Dist = FMath::Sqrt(DistSq);
						Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - Dist / Motor.Radius);
					}
				}
				else if (MotorShape == EWindMotorShape::Cylinder)
				{
					const FVector ToCell = CellCenter - Motor.WorldPosition;
					const float AxisProj = FVector::DotProduct(ToCell, Motor.Direction);
					const float HalfHeight = Motor.Height * 0.5f;

					if (FMath::Abs(AxisProj) > HalfHeight) continue;

					const float EffTopRadius = Motor.TopRadius > 0.f ? Motor.TopRadius : Motor.Radius;
					const float HeightFrac = (AxisProj + HalfHeight) / Motor.Height;
					const float LocalRadius = FMath::Lerp(Motor.Radius, EffTopRadius, HeightFrac);

					const FVector RadialVec = ToCell - Motor.Direction * AxisProj;
					const float RadialDist = RadialVec.Size();

					if (RadialDist > LocalRadius) continue;

					Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - RadialDist / LocalRadius);
				}
				else
				{
					const float DistSq = (CellCenter - Motor.WorldPosition).SizeSquared();
					if (DistSq > RadiusSq) continue;

					const float Dist = FMath::Sqrt(DistSq);
					Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - Dist / Motor.Radius);
				}

				// --- Force direction based on emission type ---
				FVector Force = FVector::ZeroVector;
				const FVector ToCell = CellCenter - Motor.WorldPosition;
				float InjectionFactor = 1.0f;

				switch (Emission)
				{
				case EWindEmissionType::Directional:
					Force = Motor.Direction * Motor.Strength * Falloff;
					break;

				case EWindEmissionType::Omni:
					Force = ToCell.GetSafeNormal() * Motor.Strength * Falloff;
					break;

				case EWindEmissionType::Vortex:
					{
						const FVector CrossDir = FVector::CrossProduct(Motor.Direction, ToCell.GetSafeNormal());
						Force = CrossDir * Motor.Strength * Motor.VortexAngularSpeed * Falloff;
					}
					break;

				case EWindEmissionType::Moving:
				{
					const FVector MoveDelta = Motor.WorldPosition - Motor.PreviousPosition;
					const float MoveDistance = MoveDelta.Size();

					// Angular velocity contribution
					const FVector AngVel = Motor.AngularVelocity;
					const float AngSpeed = AngVel.Size();
					FVector TangentialDir = FVector::ZeroVector;
					float TangentialSpeed = 0.f;

					if (AngSpeed > SMALL_NUMBER)
					{
						const FVector TangentialVel = FVector::CrossProduct(AngVel, ToCell);
						TangentialSpeed = TangentialVel.Size();
						if (TangentialSpeed > SMALL_NUMBER)
						{
							TangentialDir = TangentialVel / TangentialSpeed;
						}
					}

					const bool bHasLinearMotion = MoveDistance > 0.1f;
					const bool bHasAngularMotion = TangentialSpeed > SMALL_NUMBER;

					if (!bHasLinearMotion && !bHasAngularMotion)
					{
						Force = (Motor.Direction.IsNearlyZero() ? ToCell.GetSafeNormal() : Motor.Direction)
							* Motor.Strength * Falloff;
					}
					else if (bHasLinearMotion && !bHasAngularMotion)
					{
						const float SpeedBoost = FMath::Clamp(
							MoveDistance / FMath::Max(Motor.Radius * 0.5f, 1.f),
							0.f, 5.f);
						InjectionFactor = 1.0f + SpeedBoost * 3.0f;

						const FVector MoveDir = MoveDelta / MoveDistance;
						const FVector RadialDir = ToCell.GetSafeNormal();
						Force = (MoveDir * 0.7f + RadialDir * 0.3f).GetSafeNormal()
							* Motor.Strength * Falloff;
					}
					else if (!bHasLinearMotion && bHasAngularMotion)
					{
						const float AngularBoost = FMath::Clamp(AngSpeed / 1.f, 0.f, 5.f);
						InjectionFactor = 1.0f + AngularBoost * 2.0f;

						Force = TangentialDir * Motor.Strength * Falloff;
					}
					else
					{
						const float SpeedBoost = FMath::Clamp(
							MoveDistance / FMath::Max(Motor.Radius * 0.5f, 1.f),
							0.f, 5.f);
						const float AngularBoost = FMath::Clamp(AngSpeed / 1.f, 0.f, 5.f);
						InjectionFactor = 1.0f + SpeedBoost * 2.0f + AngularBoost * 1.5f;

						const FVector MoveDir = MoveDelta / MoveDistance;
						const FVector RadialDir = ToCell.GetSafeNormal();
						const FVector LinearDir = (MoveDir * 0.7f + RadialDir * 0.3f).GetSafeNormal();

						Force = (LinearDir * 0.6f + TangentialDir * 0.4f).GetSafeNormal()
							* Motor.Strength * Falloff;
					}
				}
				break;
				}

				const int32 Idx = CellIndex(X, Y, Z);
				if (Solids[Idx]) continue;

				Velocities[Idx] += Force * InjectionFactor * Motor.ImpulseScale;

				// Clamp velocity magnitude
				const float MaxSpeed = Motor.Strength * 2.f;
				if (Velocities[Idx].SizeSquared() > MaxSpeed * MaxSpeed)
				{
					Velocities[Idx] = Velocities[Idx].GetSafeNormal() * MaxSpeed;
				}

				Turbulences[Idx] = FMath::Clamp(Turbulences[Idx] + Falloff * 0.1f * DeltaTime, 0.f, 1.f);
			}
		}
	}
}

// ============================ Decay ============================
// Optimized with ParallelFor for large grids

void FWindGridCPU::DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime)
{
	const float Alpha = FMath::Clamp(DecayRate * DeltaTime, 0.f, 1.f);
	const float TurbDecay = FMath::Clamp(DecayRate * 0.5f * DeltaTime, 0.f, 1.f);
	const int32 Total = GetTotalCells();

	ParallelFor(Total, [&](int32 I)
	{
		if (Solids[I])
		{
			Velocities[I] = FVector::ZeroVector;
			Turbulences[I] = 0.f;
			return;
		}
		Velocities[I] = FMath::Lerp(Velocities[I], AmbientWind, Alpha);
		Turbulences[I] = FMath::Lerp(Turbulences[I], 0.f, TurbDecay);
	}, Total < ParallelMinBatch ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
}

// ============================ Diffuse ============================
// Optimized: read-only input + write-only output per iteration → ParallelFor safe

void FWindGridCPU::Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations)
{
	const float Alpha = DiffusionRate * DeltaTime;
	if (Alpha <= 0.f) return;

	Iterations = FMath::Clamp(Iterations, 1, 8);
	const int32 Total = GetTotalCells();
	const bool bParallel = Total >= ParallelMinBatch;

	for (int32 Iter = 0; Iter < Iterations; Iter++)
	{
		// Each cell reads from Velocities (input), writes to VelocitiesBack (output)
		ParallelFor(SizeZ, [&](int32 Z)
		{
			for (int32 Y = 0; Y < SizeY; Y++)
			{
				for (int32 X = 0; X < SizeX; X++)
				{
					const int32 Idx = CellIndex(X, Y, Z);

					if (Solids[Idx])
					{
						VelocitiesBack[Idx] = FVector::ZeroVector;
						continue;
					}

					const FVector& Center = Velocities[Idx];

					const int32 XpI = CellIndex(FMath::Min(X + 1, SizeX - 1), Y, Z);
					const int32 XnI = CellIndex(FMath::Max(X - 1, 0), Y, Z);
					const int32 YpI = CellIndex(X, FMath::Min(Y + 1, SizeY - 1), Z);
					const int32 YnI = CellIndex(X, FMath::Max(Y - 1, 0), Z);
					const int32 ZpI = CellIndex(X, Y, FMath::Min(Z + 1, SizeZ - 1));
					const int32 ZnI = CellIndex(X, Y, FMath::Max(Z - 1, 0));

					const FVector Xp = Solids[XpI] ? Center : Velocities[XpI];
					const FVector Xn = Solids[XnI] ? Center : Velocities[XnI];
					const FVector Yp = Solids[YpI] ? Center : Velocities[YpI];
					const FVector Yn = Solids[YnI] ? Center : Velocities[YnI];
					const FVector Zp = Solids[ZpI] ? Center : Velocities[ZpI];
					const FVector Zn = Solids[ZnI] ? Center : Velocities[ZnI];

					const FVector Laplacian = (Xp + Xn + Yp + Yn + Zp + Zn) - Center * 6.f;
					VelocitiesBack[Idx] = Center + Laplacian * Alpha;
				}
			}
		}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		SwapBuffers();
	}
}

// ============================ Advect ============================
// Forward pass: splatting with weight normalization
// Reverse pass: semi-Lagrangian backtrace (ParallelFor safe)

void FWindGridCPU::Advect(float AdvectionForce, float DeltaTime, bool bForwardPass)
{
	const float Factor = AdvectionForce * DeltaTime / CellSize;
	if (FMath::Abs(Factor) < SMALL_NUMBER) return;

	const int32 Total = GetTotalCells();
	const bool bParallel = Total >= ParallelMinBatch;

	// --- Forward advection: distribute velocity to where it's going ---
	if (bForwardPass)
	{
		FMemory::Memzero(VelocitiesBack.GetData(), VelocitiesBack.Num() * sizeof(FVector));

		FMemory::Memzero(AdvectWeights.GetData(), AdvectWeights.Num() * sizeof(float));

		// Forward splatting (not parallelizable due to write conflicts on target cells)
		for (int32 Z = 0; Z < SizeZ; Z++)
		{
			for (int32 Y = 0; Y < SizeY; Y++)
			{
				for (int32 X = 0; X < SizeX; X++)
				{
					const int32 Idx = CellIndex(X, Y, Z);
					if (Solids[Idx]) continue;

					const FVector& Vel = Velocities[Idx];

					const float TgtX = (float)X + Vel.X * Factor;
					const float TgtY = (float)Y + Vel.Y * Factor;
					const float TgtZ = (float)Z + Vel.Z * Factor;

					const int32 X0 = FMath::FloorToInt(TgtX);
					const int32 Y0 = FMath::FloorToInt(TgtY);
					const int32 Z0 = FMath::FloorToInt(TgtZ);

					const float Fx = TgtX - X0;
					const float Fy = TgtY - Y0;
					const float Fz = TgtZ - Z0;

					for (int32 Dz = 0; Dz <= 1; Dz++)
					{
						for (int32 Dy = 0; Dy <= 1; Dy++)
						{
							for (int32 Dx = 0; Dx <= 1; Dx++)
							{
								const int32 Cx = X0 + Dx;
								const int32 Cy = Y0 + Dy;
								const int32 Cz = Z0 + Dz;

								if (!IsInBounds(Cx, Cy, Cz)) continue;
								if (Solids[CellIndex(Cx, Cy, Cz)]) continue;

								const float Wx = Dx ? Fx : (1.f - Fx);
								const float Wy = Dy ? Fy : (1.f - Fy);
								const float Wz = Dz ? Fz : (1.f - Fz);
								const float W = Wx * Wy * Wz;

								const int32 TgtIdx = CellIndex(Cx, Cy, Cz);
								VelocitiesBack[TgtIdx] += Vel * W;
								AdvectWeights[TgtIdx] += W;
							}
						}
					}
				}
			}
		}

		// Normalize
		for (int32 I = 0; I < Total; I++)
		{
			if (AdvectWeights[I] > SMALL_NUMBER)
			{
				VelocitiesBack[I] /= AdvectWeights[I];
			}
			else
			{
				VelocitiesBack[I] = Velocities[I];
			}
		}

		SwapBuffers();
	}

	// --- Reverse advection (semi-Lagrangian backtrace) — ParallelFor safe ---
	ParallelFor(SizeZ, [&](int32 Z)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				const int32 Idx = CellIndex(X, Y, Z);

				if (Solids[Idx])
				{
					VelocitiesBack[Idx] = FVector::ZeroVector;
					continue;
				}

				const FVector& Vel = Velocities[Idx];

				const float SrcX = (float)X - Vel.X * Factor;
				const float SrcY = (float)Y - Vel.Y * Factor;
				const float SrcZ = (float)Z - Vel.Z * Factor;

				VelocitiesBack[Idx] = SampleVelocityAtLocal(SrcX, SrcY, SrcZ);
			}
		}
	}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	SwapBuffers();
}

// ============================ Pressure Projection ============================

void FWindGridCPU::ProjectPressure(int32 Iterations)
{
	const int32 Total = GetTotalCells();
	TArray<float> Pressure;
	Pressure.SetNumZeroed(Total);

	auto VX = [&](int32 Nx, int32 Ny, int32 Nz) -> float
	{
		if (!IsInBounds(Nx, Ny, Nz) || Solids[CellIndex(Nx, Ny, Nz)]) return 0.f;
		return Velocities[CellIndex(Nx, Ny, Nz)].X;
	};
	auto VY = [&](int32 Nx, int32 Ny, int32 Nz) -> float
	{
		if (!IsInBounds(Nx, Ny, Nz) || Solids[CellIndex(Nx, Ny, Nz)]) return 0.f;
		return Velocities[CellIndex(Nx, Ny, Nz)].Y;
	};
	auto VZ = [&](int32 Nx, int32 Ny, int32 Nz) -> float
	{
		if (!IsInBounds(Nx, Ny, Nz) || Solids[CellIndex(Nx, Ny, Nz)]) return 0.f;
		return Velocities[CellIndex(Nx, Ny, Nz)].Z;
	};

	// Step 1: Gauss-Seidel pressure solve (sequential — Gauss-Seidel is inherently serial)
	for (int32 Iter = 0; Iter < Iterations; Iter++)
	{
		for (int32 Z = 0; Z < SizeZ; Z++)
		{
			for (int32 Y = 0; Y < SizeY; Y++)
			{
				for (int32 X = 0; X < SizeX; X++)
				{
					const int32 Idx = CellIndex(X, Y, Z);
					if (Solids[Idx]) continue;

					const float Div =
						(VX(X + 1, Y, Z) - VX(X - 1, Y, Z)) * 0.5f +
						(VY(X, Y + 1, Z) - VY(X, Y - 1, Z)) * 0.5f +
						(VZ(X, Y, Z + 1) - VZ(X, Y, Z - 1)) * 0.5f;

					float SumP = 0.f;
					int32 OpenCount = 0;

					auto AddNeighborP = [&](int32 Nx, int32 Ny, int32 Nz)
					{
						if (!IsInBounds(Nx, Ny, Nz) || Solids[CellIndex(Nx, Ny, Nz)])
						{
							SumP += Pressure[Idx]; // Neumann BC
						}
						else
						{
							SumP += Pressure[CellIndex(Nx, Ny, Nz)];
						}
						OpenCount++;
					};

					AddNeighborP(X + 1, Y, Z);
					AddNeighborP(X - 1, Y, Z);
					AddNeighborP(X, Y + 1, Z);
					AddNeighborP(X, Y - 1, Z);
					AddNeighborP(X, Y, Z + 1);
					AddNeighborP(X, Y, Z - 1);

					if (OpenCount > 0)
					{
						Pressure[Idx] = (SumP - Div) / (float)OpenCount;
					}
				}
			}
		}
	}

	// Step 2: Subtract pressure gradient — ParallelFor safe (read Pressure, write Velocities)
	const bool bParallel = Total >= ParallelMinBatch;
	ParallelFor(SizeZ, [&](int32 Z)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				const int32 Idx = CellIndex(X, Y, Z);
				if (Solids[Idx]) continue;

				auto P = [&](int32 Nx, int32 Ny, int32 Nz) -> float
				{
					if (!IsInBounds(Nx, Ny, Nz) || Solids[CellIndex(Nx, Ny, Nz)])
						return Pressure[Idx];
					return Pressure[CellIndex(Nx, Ny, Nz)];
				};

				Velocities[Idx].X -= (P(X + 1, Y, Z) - P(X - 1, Y, Z)) * 0.5f;
				Velocities[Idx].Y -= (P(X, Y + 1, Z) - P(X, Y - 1, Z)) * 0.5f;
				Velocities[Idx].Z -= (P(X, Y, Z + 1) - P(X, Y, Z - 1)) * 0.5f;
			}
		}
	}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
}

// ============================ Boundary Fade ============================

void FWindGridCPU::BoundaryFadeOut(int32 FadeCells)
{
	FadeCells = FMath::Clamp(FadeCells, 1, FMath::Min3(SizeX, SizeY, SizeZ) / 2);
	const float InvFade = 1.f / (float)FadeCells;
	const bool bParallel = GetTotalCells() >= ParallelMinBatch;

	ParallelFor(SizeZ, [&](int32 Z)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				const float Dx = FMath::Min((float)X, (float)(SizeX - 1 - X));
				const float Dy = FMath::Min((float)Y, (float)(SizeY - 1 - Y));
				const float Dz = FMath::Min((float)Z, (float)(SizeZ - 1 - Z));

				const float EdgeDist = FMath::Min3(Dx, Dy, Dz);
				if (EdgeDist >= FadeCells) continue;

				const float Alpha = FMath::SmoothStep(0.f, 1.f, EdgeDist * InvFade);
				const int32 Idx = CellIndex(X, Y, Z);
				Velocities[Idx] *= Alpha;
				Turbulences[Idx] *= Alpha;
			}
		}
	}, !bParallel ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);
}

// ============================ Shift Data ============================

void FWindGridCPU::ShiftData(FIntVector CellOffset, FVector AmbientWind)
{
	if (CellOffset.X == 0 && CellOffset.Y == 0 && CellOffset.Z == 0) return;

	const int32 Total = GetTotalCells();

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
