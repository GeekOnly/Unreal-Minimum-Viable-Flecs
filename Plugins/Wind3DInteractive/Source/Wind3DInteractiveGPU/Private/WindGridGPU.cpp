#include "WindGridGPU.h"
#include "WindComponents.h"
#include "WindTypes.h"

// ============================================================
// GPU Solver Skeleton
// Currently delegates to CPU-equivalent logic as placeholder.
// TODO: Replace with RHI Compute Shader dispatch.
// ============================================================

void FWindGridGPU::Initialize(int32 InSizeX, int32 InSizeY, int32 InSizeZ, float InCellSize)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	SizeZ = InSizeZ;
	CellSize = InCellSize;

	const int32 Total = GetTotalCells();
	ReadbackVelocities.SetNumZeroed(Total);
	ReadbackTurbulences.SetNumZeroed(Total);
	Solids.SetNumZeroed(Total);

	// TODO: Create GPU textures (RWTexture3D<float4> ping/pong)
	// TODO: Create solids texture
	// TODO: Create motor structured buffer
}

void FWindGridGPU::Reset()
{
	const int32 Total = GetTotalCells();
	FMemory::Memzero(ReadbackVelocities.GetData(), Total * sizeof(FVector));
	FMemory::Memzero(ReadbackTurbulences.GetData(), Total * sizeof(float));
	FMemory::Memzero(Solids.GetData(), Total * sizeof(uint8));
	// TODO: Clear GPU textures
}

// ---- Simulation Pipeline (TODO: dispatch compute shaders) ----

void FWindGridGPU::InjectMotor(const FWindMotorData& Motor, float DeltaTime)
{
	// TODO: Upload motor data to StructuredBuffer, dispatch WindInjectMotor.usf
	// For now, accumulate motor data and batch-dispatch at end of frame
}

void FWindGridGPU::DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime)
{
	// TODO: Dispatch WindDecay.usf compute shader
}

void FWindGridGPU::Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations)
{
	// TODO: Dispatch WindDiffuse.usf compute shader (Iterations times, ping-pong)
}

void FWindGridGPU::Advect(float AdvectionForce, float DeltaTime, bool bForwardPass)
{
	// TODO: Dispatch WindAdvect.usf compute shader
}

void FWindGridGPU::ProjectPressure(int32 Iterations)
{
	// TODO: Dispatch WindPressure.usf compute shader (Jacobi iterations)
}

void FWindGridGPU::BoundaryFadeOut(int32 FadeCells)
{
	// TODO: Dispatch WindBoundary.usf compute shader
}

void FWindGridGPU::ShiftData(FIntVector CellOffset, FVector AmbientWind)
{
	// TODO: Dispatch WindShift.usf compute shader
}

// ---- Query (uses CPU readback buffer) ----

FVector FWindGridGPU::SampleVelocityAt(const FVector& WorldPos) const
{
	// Uses readback buffer (1-frame latency)
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

				Result += ReadbackVelocities[CellIndex(Cx, Cy, Cz)] * Wx * Wy * Wz;
			}
		}
	}
	return Result;
}

FVector FWindGridGPU::SampleVelocityAtLocal(float Lx, float Ly, float Lz) const
{
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

				Result += ReadbackVelocities[CellIndex(Cx, Cy, Cz)] * Wx * Wy * Wz;
			}
		}
	}
	return Result;
}

float FWindGridGPU::SampleTurbulenceAt(const FVector& WorldPos) const
{
	const int32 Idx = WorldToIndex(WorldPos);
	if (Idx < 0) return 0.f;
	return ReadbackTurbulences[Idx];
}

// ---- Obstacle Grid ----

void FWindGridGPU::ClearSolids()
{
	FMemory::Memzero(Solids.GetData(), Solids.Num() * sizeof(uint8));
}

void FWindGridGPU::MarkSolid(int32 X, int32 Y, int32 Z)
{
	if (IsInBounds(X, Y, Z))
	{
		Solids[CellIndex(X, Y, Z)] = 1;
	}
}

bool FWindGridGPU::IsSolid(int32 X, int32 Y, int32 Z) const
{
	if (!IsInBounds(X, Y, Z)) return false;
	return Solids[CellIndex(X, Y, Z)] != 0;
}

// ---- Coordinate Helpers ----

int32 FWindGridGPU::CellIndex(int32 X, int32 Y, int32 Z) const
{
	return X + Y * SizeX + Z * SizeX * SizeY;
}

bool FWindGridGPU::IsInBounds(int32 X, int32 Y, int32 Z) const
{
	return X >= 0 && X < SizeX && Y >= 0 && Y < SizeY && Z >= 0 && Z < SizeZ;
}

FIntVector FWindGridGPU::WorldToCell(const FVector& WorldPos) const
{
	const FVector Local = WorldPos - WorldOrigin;
	return FIntVector(
		FMath::FloorToInt(Local.X / CellSize),
		FMath::FloorToInt(Local.Y / CellSize),
		FMath::FloorToInt(Local.Z / CellSize)
	);
}

int32 FWindGridGPU::WorldToIndex(const FVector& WorldPos) const
{
	const FIntVector Cell = WorldToCell(WorldPos);
	if (!IsInBounds(Cell.X, Cell.Y, Cell.Z)) return -1;
	return CellIndex(Cell.X, Cell.Y, Cell.Z);
}

FVector FWindGridGPU::CellToWorld(int32 X, int32 Y, int32 Z) const
{
	return WorldOrigin + FVector(
		(X + 0.5f) * CellSize,
		(Y + 0.5f) * CellSize,
		(Z + 0.5f) * CellSize
	);
}
