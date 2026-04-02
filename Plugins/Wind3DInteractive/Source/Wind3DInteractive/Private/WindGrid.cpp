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

	const float RadiusSq = Motor.Radius * Motor.Radius;
	const float SafeRadius = FMath::Max(Motor.Radius, 1.f);

	const FVector MoveDeltaRaw = Motor.WorldPosition - Motor.PreviousPosition;
	const float MoveDistanceRaw = MoveDeltaRaw.Size();
	const FVector MoveDirRaw = MoveDistanceRaw > SMALL_NUMBER
		? MoveDeltaRaw / MoveDistanceRaw
		: (Motor.Direction.IsNearlyZero() ? FVector::ForwardVector : Motor.Direction.GetSafeNormal());

	// Explicit trail segment for Moving motors: extends backward from current position.
	const float EffectiveTrailLength = (Emission == EWindEmissionType::Moving)
		? FMath::Max(Motor.MoveLength, MoveDistanceRaw)
		: 0.f;
	const FVector TrailEnd = Motor.WorldPosition;
	const FVector TrailStart = TrailEnd - MoveDirRaw * EffectiveTrailLength;
	const FVector TrailDelta = TrailEnd - TrailStart;
	const float TrailLenSq = TrailDelta.SizeSquared();

	// Bounding box in world space
	FVector BoundsMin, BoundsMax;
	if (Emission == EWindEmissionType::Moving)
	{
		// Union of trail endpoints to include explicit MoveLength influence.
		BoundsMin = FVector::Min(TrailStart, TrailEnd) - FVector(BoundsExtent);
		BoundsMax = FVector::Max(TrailStart, TrailEnd) + FVector(BoundsExtent);
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
						// Cylinder shape: use standard cylinder containment logic
						// (Using current position only - accurate for rotation. Linear trails for cylinders are approximated)
						const FVector ToCell = CellCenter - Motor.WorldPosition;
						const float AxisProj = FVector::DotProduct(ToCell, Motor.Direction);
						const float HalfHeight = Motor.Height * 0.5f;

						if (FMath::Abs(AxisProj) > HalfHeight) continue;

						// Interpolated radius along axis
						const float EffTopRadius = Motor.TopRadius > 0.f ? Motor.TopRadius : Motor.Radius;
						const float HeightFrac = (AxisProj + HalfHeight) / Motor.Height;
						const float LocalRadius = FMath::Lerp(Motor.Radius, EffTopRadius, HeightFrac);

						// Radial distance
						const FVector RadialVec = ToCell - Motor.Direction * AxisProj;
						const float RadialDist = RadialVec.Size();

						if (RadialDist > LocalRadius) continue;

						Falloff = FMath::SmoothStep(0.f, 1.f, 1.f - RadialDist / LocalRadius);
					}
					else
					{
						// Moving motor: capsule along explicit trail segment.
						// Project onto line from TrailStart to TrailEnd.

						FVector ClosestPoint;
						float TrailT = 1.f;
						if (TrailLenSq < SMALL_NUMBER)
						{
							ClosestPoint = Motor.WorldPosition;
						}
						else
						{
							TrailT = FMath::Clamp(
								FVector::DotProduct(CellCenter - TrailStart, TrailDelta) / TrailLenSq,
								0.f, 1.f);
							ClosestPoint = TrailStart + TrailDelta * TrailT;
						}

						const float DistSq = (CellCenter - ClosestPoint).SizeSquared();
						if (DistSq > RadiusSq) continue;

						const float Dist = FMath::Sqrt(DistSq);
						const float RadialFalloff = FMath::SmoothStep(0.f, 1.f, 1.f - Dist / SafeRadius);
						const float TrailFalloff = FMath::Lerp(0.35f, 1.f, TrailT);
						Falloff = RadialFalloff * TrailFalloff;
					}
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

				// InjectionFactor = 1.0: direct velocity addition per frame (matches GoW reference).
				// DeltaTime NOT used — inject the full Strength value directly so the effect is visible.
				// DecayToAmbient() counterbalances (tune DecayRate in WindFieldSetupActor for persistence).
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
					const FVector MoveDelta = MoveDeltaRaw;
					const float MoveDistance = MoveDistanceRaw;
					const float LinearSpeed = MoveDistance / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);

					// --- Angular velocity contribution ---
					// Tangential velocity at this cell: V_tan = ω × r
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
						// Stationary + no rotation → fallback to directional
						Force = (Motor.Direction.IsNearlyZero() ? ToCell.GetSafeNormal() : Motor.Direction)
							* Motor.Strength * Falloff;
					}
					else if (bHasLinearMotion && !bHasAngularMotion)
					{
						// Linear motion only (original behavior)
						const float SpeedBoost = FMath::Clamp(
							LinearSpeed / FMath::Max(SafeRadius, 50.f),
							0.f, 5.f);
						InjectionFactor = 1.0f + SpeedBoost * 3.0f;

						const FVector MoveDir = MoveDelta / MoveDistance;
						const FVector RadialDir = ToCell.GetSafeNormal();
						Force = (MoveDir * 0.7f + RadialDir * 0.3f).GetSafeNormal()
							* Motor.Strength * Falloff;
					}
					else if (!bHasLinearMotion && bHasAngularMotion)
					{
						// Rotation only — tangential wind from ω × r
						// Use AngSpeed (rad/s) as boost base — radius-independent
						// At 1 rad/s => boost factor 1. At 5+ rad/s => max boost.
						const float AngularBoost = FMath::Clamp(AngSpeed / 1.f, 0.f, 5.f);
						InjectionFactor = 1.0f + AngularBoost * 2.0f;

						Force = TangentialDir * Motor.Strength * Falloff;
					}
					else
					{
						// Both linear + angular — blend
						const float SpeedBoost = FMath::Clamp(
							LinearSpeed / FMath::Max(SafeRadius, 50.f),
							0.f, 5.f);
						// AngSpeed-based boost (radius-independent, same as rotation-only branch)
						const float AngularBoost = FMath::Clamp(AngSpeed / 1.f, 0.f, 5.f);
						InjectionFactor = 1.0f + SpeedBoost * 2.0f + AngularBoost * 1.5f;

						const FVector MoveDir = MoveDelta / MoveDistance;
						const FVector RadialDir = ToCell.GetSafeNormal();
						const FVector LinearDir = (MoveDir * 0.7f + RadialDir * 0.3f).GetSafeNormal();

						// Blend: 60% linear + 40% tangential
						Force = (LinearDir * 0.6f + TangentialDir * 0.4f).GetSafeNormal()
							* Motor.Strength * Falloff;
					}
				}
				break;
				}

				const int32 Idx = CellIndex(X, Y, Z);
				if (Solids[Idx]) continue; // Don't inject into solid cells

				// If force points into nearby solid cells, bend it along obstacle tangent.
				if (!Force.IsNearlyZero())
				{
					const auto IsSolidSafe = [this](int32 Cx, int32 Cy, int32 Cz) -> bool
					{
						return IsInBounds(Cx, Cy, Cz) && IsSolid(Cx, Cy, Cz);
					};

					const FVector SolidNormal(
						(IsSolidSafe(X - 1, Y, Z) ? 1.f : 0.f) - (IsSolidSafe(X + 1, Y, Z) ? 1.f : 0.f),
						(IsSolidSafe(X, Y - 1, Z) ? 1.f : 0.f) - (IsSolidSafe(X, Y + 1, Z) ? 1.f : 0.f),
						(IsSolidSafe(X, Y, Z - 1) ? 1.f : 0.f) - (IsSolidSafe(X, Y, Z + 1) ? 1.f : 0.f)
					);

					if (!SolidNormal.IsNearlyZero())
					{
						const FVector N = SolidNormal.GetSafeNormal();
						const float IntoObstacle = FVector::DotProduct(Force.GetSafeNormal(), -N);
						if (IntoObstacle > 0.05f)
						{
							FVector Deflected = FVector::VectorPlaneProject(Force, N);
							if (!Deflected.IsNearlyZero())
							{
								Deflected = Deflected.GetSafeNormal() * Force.Size();
								Force = FMath::Lerp(Force, Deflected, 0.6f);
							}
						}
					}
				}

				Velocities[Idx] += Force * InjectionFactor * Motor.ImpulseScale;

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

void FWindGrid::ProjectPressure(int32 Iterations)
{
	// Pressure projection (stable fluids, Gauss-Seidel Poisson solve)
	// Makes velocity field divergence-free so wind flows AROUND obstacles
	// instead of piling up at them or stopping dead.
	//
	// Algorithm:
	//   1. Solve ∇²p = ∇·v  (Poisson eq — find pressure from divergence)
	//   2. v -= ∇p          (subtract gradient — removes divergence)
	//
	// Boundary conditions:
	//   Velocity: no flow into solid (Dirichlet, v_solid = 0)
	//   Pressure: no gradient through solid (Neumann, p_solid = p_fluid)

	const int32 Total = GetTotalCells();
	TArray<float> Pressure;
	Pressure.SetNumZeroed(Total);

	// Helper: safe velocity component at neighbor, zero if solid/OOB
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

	// --- Step 1: Gauss-Seidel pressure solve ---
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

					// Velocity divergence at this cell (central difference)
					// Positive divergence = wind flowing out = excess pressure
					const float Div =
						(VX(X + 1, Y, Z) - VX(X - 1, Y, Z)) * 0.5f +
						(VY(X, Y + 1, Z) - VY(X, Y - 1, Z)) * 0.5f +
						(VZ(X, Y, Z + 1) - VZ(X, Y, Z - 1)) * 0.5f;

					// Sum of open neighbor pressures (Neumann BC: solid mirrors pressure)
					float SumP = 0.f;
					int32 OpenCount = 0;

					auto AddNeighborP = [&](int32 Nx, int32 Ny, int32 Nz)
					{
						if (!IsInBounds(Nx, Ny, Nz) || Solids[CellIndex(Nx, Ny, Nz)])
						{
							SumP += Pressure[Idx]; // Neumann: dp/dn = 0 -> mirror
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

	// --- Step 2: Subtract pressure gradient from velocity ---
	// This makes the field divergence-free — wind redirects around obstacles
	for (int32 Z = 0; Z < SizeZ; Z++)
	{
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				const int32 Idx = CellIndex(X, Y, Z);
				if (Solids[Idx]) continue;

				// Pressure at neighbor — mirror pressure at walls (Neumann BC)
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
	}
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
