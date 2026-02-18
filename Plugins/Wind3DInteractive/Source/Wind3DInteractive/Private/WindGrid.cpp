#include "WindGrid.h"
#include "WindComponents.h"
#include "WindTypes.h"

void FWindGrid::Initialize()
{
	const int32 Total = GetTotalCells();
	Velocities.SetNumZeroed(Total);
	VelocitiesBack.SetNumZeroed(Total);
	Turbulences.SetNumZeroed(Total);
	Solids.SetNumZeroed(Total);
}

void FWindGrid::Reset()
{
	FMemory::Memzero(Velocities.GetData(), Velocities.Num() * sizeof(FVector));
	FMemory::Memzero(VelocitiesBack.GetData(), VelocitiesBack.Num() * sizeof(FVector));
	FMemory::Memzero(Turbulences.GetData(), Turbulences.Num() * sizeof(float));
	FMemory::Memzero(Solids.GetData(), Solids.Num() * sizeof(uint8));
}

void FWindGrid::ClearSolids()
{
	FMemory::Memzero(Solids.GetData(), Solids.Num() * sizeof(uint8));
}

void FWindGrid::MarkSolid(int32 X, int32 Y, int32 Z)
{
	if (IsInBounds(X, Y, Z))
	{
		Solids[CellIndex(X, Y, Z)] = 1;
	}
}

bool FWindGrid::IsSolid(int32 X, int32 Y, int32 Z) const
{
	if (!IsInBounds(X, Y, Z)) return false;
	return Solids[CellIndex(X, Y, Z)] != 0;
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
		// Extend bounds to cover the trail from previous position
		BoundsExtent = FMath::Max(BoundsExtent, Motor.MoveLength + Motor.Radius);
	}

	// Bounding box in world space
	FVector BoundsMin, BoundsMax;
	if (Emission == EWindEmissionType::Moving)
	{
		// Union of current + previous position
		BoundsMin = FVector::Min(Motor.WorldPosition, Motor.PreviousPosition) - FVector(Motor.Radius);
		BoundsMax = FVector::Max(Motor.WorldPosition, Motor.PreviousPosition) + FVector(Motor.Radius);
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
					// Moving motor: capsule along movement path
					// Project cell onto line segment from PreviousPosition to WorldPosition
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
				else if (MotorShape == EWindMotorShape::Cylinder)
				{
					// Cylinder shape: project onto axis, check height + interpolated radius
					const FVector ToCell = CellCenter - Motor.WorldPosition;
					const float AxisProj = FVector::DotProduct(ToCell, Motor.Direction);
					const float HalfHeight = Motor.Height * 0.5f;

					if (FMath::Abs(AxisProj) > HalfHeight) continue;

					// Interpolated radius along axis: bottom (Radius) to top (TopRadius)
					const float EffTopRadius = Motor.TopRadius > 0.f ? Motor.TopRadius : Motor.Radius;
					const float HeightFrac = (AxisProj + HalfHeight) / Motor.Height; // 0 = bottom, 1 = top
					const float LocalRadius = FMath::Lerp(Motor.Radius, EffTopRadius, HeightFrac);

					// Radial distance (perpendicular to axis)
					const FVector RadialVec = ToCell - Motor.Direction * AxisProj;
					const float RadialDist = RadialVec.Size();

					if (RadialDist > LocalRadius) continue;

					Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - RadialDist / LocalRadius);
				}
				else
				{
					// Sphere shape (default)
					const float DistSq = (CellCenter - Motor.WorldPosition).SizeSquared();
					if (DistSq > RadiusSq) continue;

					const float Dist = FMath::Sqrt(DistSq);
					Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - Dist / Motor.Radius);
				}

				// --- Force direction based on emission type ---
				FVector Force = FVector::ZeroVector;
				const FVector ToCell = CellCenter - Motor.WorldPosition;

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
						// GoW-style: blend radial push + movement direction
						const FVector MoveDir = (Motor.WorldPosition - Motor.PreviousPosition).GetSafeNormal();
						const FVector RadialDir = ToCell.GetSafeNormal();

						// If object barely moved, fallback to omni
						FVector BlowDir;
						if (MoveDir.SizeSquared() < SMALL_NUMBER)
						{
							BlowDir = RadialDir;
						}
						else
						{
							BlowDir = (RadialDir + MoveDir).GetSafeNormal();
						}
						Force = BlowDir * Motor.Strength * Falloff;
					}
					break;
				}

				const int32 Idx = CellIndex(X, Y, Z);
				if (Solids[Idx]) continue; // Don't inject into solid cells

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
		if (Solids[I])
		{
			Velocities[I] = FVector::ZeroVector;
			Turbulences[I] = 0.f;
			continue;
		}
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

void FWindGrid::Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations)
{
	// GoW-style 3D diffusion: 6-neighbor Laplacian blur
	// Multiple iterations produce smoother, more physically accurate spreading
	const float Alpha = DiffusionRate * DeltaTime;
	if (Alpha <= 0.f) return;

	Iterations = FMath::Clamp(Iterations, 1, 8);

	for (int32 Iter = 0; Iter < Iterations; Iter++)
	{
		for (int32 Z = 0; Z < SizeZ; Z++)
		{
			for (int32 Y = 0; Y < SizeY; Y++)
			{
				for (int32 X = 0; X < SizeX; X++)
				{
					const int32 Idx = CellIndex(X, Y, Z);

					// Solid cells stay at zero velocity
					if (Solids[Idx])
					{
						VelocitiesBack[Idx] = FVector::ZeroVector;
						continue;
					}

					const FVector& Center = Velocities[Idx];

					// Sample 6 neighbors — if neighbor is solid, reflect (use Center instead)
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
		}

		SwapBuffers();
	}
}

void FWindGrid::Advect(float AdvectionForce, float DeltaTime, bool bForwardPass)
{
	const float Factor = AdvectionForce * DeltaTime / CellSize;
	if (FMath::Abs(Factor) < SMALL_NUMBER) return;

	// --- Forward advection: distribute velocity to where it's going ---
	// More accurate for fast-moving wind; prevents "vacuum" artifacts
	if (bForwardPass)
	{
		// Zero out back buffer for accumulation
		FMemory::Memzero(VelocitiesBack.GetData(), VelocitiesBack.Num() * sizeof(FVector));

		// Weight buffer tracks how much total weight each target cell received
		TArray<float> Weights;
		Weights.SetNumZeroed(GetTotalCells());

		for (int32 Z = 0; Z < SizeZ; Z++)
		{
			for (int32 Y = 0; Y < SizeY; Y++)
			{
				for (int32 X = 0; X < SizeX; X++)
				{
					const int32 Idx = CellIndex(X, Y, Z);
					if (Solids[Idx]) continue; // Don't splat from solid cells

					const FVector& Vel = Velocities[Idx];

					// Target position: where does this cell's wind go?
					const float TgtX = (float)X + Vel.X * Factor;
					const float TgtY = (float)Y + Vel.Y * Factor;
					const float TgtZ = (float)Z + Vel.Z * Factor;

					// Distribute to 8 surrounding cells (trilinear splat)
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
								if (Solids[CellIndex(Cx, Cy, Cz)]) continue; // Don't splat into solid

								const float Wx = Dx ? Fx : (1.f - Fx);
								const float Wy = Dy ? Fy : (1.f - Fy);
								const float Wz = Dz ? Fz : (1.f - Fz);
								const float W = Wx * Wy * Wz;

								const int32 TgtIdx = CellIndex(Cx, Cy, Cz);
								VelocitiesBack[TgtIdx] += Vel * W;
								Weights[TgtIdx] += W;
							}
						}
					}
				}
			}
		}

		// Normalize: cells that received weight > 0 use averaged velocity
		// Cells with no contribution keep original velocity (prevents holes)
		for (int32 I = 0; I < GetTotalCells(); I++)
		{
			if (Weights[I] > SMALL_NUMBER)
			{
				VelocitiesBack[I] /= Weights[I];
			}
			else
			{
				VelocitiesBack[I] = Velocities[I];
			}
		}

		SwapBuffers();
	}

	// --- Reverse advection (semi-Lagrangian backtrace) ---
	for (int32 Z = 0; Z < SizeZ; Z++)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				const int32 Idx = CellIndex(X, Y, Z);

				// Solid cells stay at zero
				if (Solids[Idx])
				{
					VelocitiesBack[Idx] = FVector::ZeroVector;
					continue;
				}

				const FVector& Vel = Velocities[Idx];

				// Backtrace: where did this cell's wind come from?
				const float SrcX = (float)X - Vel.X * Factor;
				const float SrcY = (float)Y - Vel.Y * Factor;
				const float SrcZ = (float)Z - Vel.Z * Factor;

				VelocitiesBack[Idx] = SampleVelocityAtLocal(SrcX, SrcY, SrcZ);
			}
		}
	}

	SwapBuffers();
}

void FWindGrid::BoundaryFadeOut(int32 FadeCells)
{
	// Reduce wind strength at grid edges to prevent hard cutoff artifacts
	FadeCells = FMath::Clamp(FadeCells, 1, FMath::Min3(SizeX, SizeY, SizeZ) / 2);
	const float InvFade = 1.f / (float)FadeCells;

	for (int32 Z = 0; Z < SizeZ; Z++)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				// Distance from each edge (in cells)
				const float Dx = FMath::Min((float)X, (float)(SizeX - 1 - X));
				const float Dy = FMath::Min((float)Y, (float)(SizeY - 1 - Y));
				const float Dz = FMath::Min((float)Z, (float)(SizeZ - 1 - Z));

				// Minimum distance to any edge
				const float EdgeDist = FMath::Min3(Dx, Dy, Dz);

				if (EdgeDist >= FadeCells) continue;

				// Smooth fade: 0 at edge -> 1 at FadeCells distance
				const float Alpha = FMath::SmoothStep(0.f, 1.f, EdgeDist * InvFade);
				const int32 Idx = CellIndex(X, Y, Z);
				Velocities[Idx] *= Alpha;
				Turbulences[Idx] *= Alpha;
			}
		}
	}
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
