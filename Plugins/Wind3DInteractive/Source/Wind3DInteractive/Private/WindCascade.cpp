#include "WindCascade.h"
#include "WindComponents.h"
#include "Wind3DInteractiveModule.h"

FWindCascade::FWindCascade()
{
}

FWindCascade::~FWindCascade()
{
	Shutdown();
}

void FWindCascade::Initialize(const TArray<FWindCascadeConfig>& Configs, bool bUseGPU)
{
	Shutdown();

	Levels.SetNum(Configs.Num());
	for (int32 i = 0; i < Configs.Num(); i++)
	{
		FWindCascadeLevel& Level = Levels[i];
		Level.Config = Configs[i];
		Level.AccumulatedDelta = FVector::ZeroVector;

		// Create solver via factory
		if (bUseGPU && FWindSolverFactory::CreateGPU.IsBound())
		{
			Level.Solver = FWindSolverFactory::CreateGPU.Execute();
		}
		else
		{
			check(FWindSolverFactory::CreateCPU.IsBound());
			Level.Solver = FWindSolverFactory::CreateCPU.Execute();
		}

		Level.Solver->Initialize(
			Level.Config.SizeX,
			Level.Config.SizeY,
			Level.Config.SizeZ,
			Level.Config.CellSize);

		UE_LOG(LogWind3D, Log, TEXT("Cascade level %d: CellSize=%.0f, Grid=%dx%dx%d, Coverage=%.0fx%.0fx%.0f"),
			i, Level.Config.CellSize,
			Level.Config.SizeX, Level.Config.SizeY, Level.Config.SizeZ,
			Level.Config.GetCoverage().X, Level.Config.GetCoverage().Y, Level.Config.GetCoverage().Z);
	}

	UE_LOG(LogWind3D, Log, TEXT("WindCascade initialized with %d levels"), Levels.Num());
}

void FWindCascade::Shutdown()
{
	Levels.Empty();
}

// ---- Simulation Pipeline ----

bool FWindCascade::MotorOverlapsLevel(const FWindMotorData& Motor, int32 LevelIndex) const
{
	const FWindCascadeLevel& Level = Levels[LevelIndex];
	const IWindSolver& S = *Level.Solver;

	// Grid AABB
	const FVector GridMin = S.GetWorldOrigin();
	const FVector GridMax = GridMin + FVector(
		S.GetSizeX() * S.GetCellSize(),
		S.GetSizeY() * S.GetCellSize(),
		S.GetSizeZ() * S.GetCellSize());

	// Motor sphere bounding (conservative — works for all shapes)
	const float MotorExtent = FMath::Max(Motor.Radius, Motor.Height);
	const FVector MotorMin = Motor.WorldPosition - FVector(MotorExtent);
	const FVector MotorMax = Motor.WorldPosition + FVector(MotorExtent);

	// AABB overlap test
	return !(MotorMax.X < GridMin.X || MotorMin.X > GridMax.X ||
	         MotorMax.Y < GridMin.Y || MotorMin.Y > GridMax.Y ||
	         MotorMax.Z < GridMin.Z || MotorMin.Z > GridMax.Z);
}

void FWindCascade::InjectMotor(const FWindMotorData& Motor, float DeltaTime)
{
	if (!Motor.bEnabled || Motor.Strength <= 0.f) return;

	for (int32 i = 0; i < Levels.Num(); i++)
	{
		if (MotorOverlapsLevel(Motor, i))
		{
			Levels[i].Solver->InjectMotor(Motor, DeltaTime);
		}
	}
}

void FWindCascade::DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime)
{
	for (auto& Level : Levels)
	{
		Level.Solver->DecayToAmbient(AmbientWind, DecayRate, DeltaTime);
	}
}

void FWindCascade::Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations)
{
	for (auto& Level : Levels)
	{
		Level.Solver->Diffuse(DiffusionRate, DeltaTime, Iterations);
	}
}

void FWindCascade::Advect(float AdvectionForce, float DeltaTime, bool bForwardPass)
{
	for (auto& Level : Levels)
	{
		Level.Solver->Advect(AdvectionForce, DeltaTime, bForwardPass);
	}
}

void FWindCascade::ProjectPressure(int32 Iterations)
{
	for (auto& Level : Levels)
	{
		Level.Solver->ProjectPressure(Iterations);
	}
}

void FWindCascade::BoundaryFadeOut(int32 FadeCells)
{
	for (auto& Level : Levels)
	{
		Level.Solver->BoundaryFadeOut(FadeCells);
	}
}

// ---- Grid Following ----

void FWindCascade::SetWorldOriginCentered(const FVector& CenterPos)
{
	for (auto& Level : Levels)
	{
		const FVector HalfExtent = Level.Config.GetCoverage() * 0.5f;
		Level.Solver->SetWorldOrigin(CenterPos - HalfExtent);
		Level.AccumulatedDelta = FVector::ZeroVector;
	}
}

void FWindCascade::ShiftAllGrids(const FVector& WorldDelta, const FVector& AmbientWind)
{
	for (auto& Level : Levels)
	{
		Level.AccumulatedDelta += WorldDelta;

		const float CS = Level.Config.CellSize;
		const FIntVector CellOffset(
			FMath::FloorToInt(Level.AccumulatedDelta.X / CS),
			FMath::FloorToInt(Level.AccumulatedDelta.Y / CS),
			FMath::FloorToInt(Level.AccumulatedDelta.Z / CS));

		if (CellOffset.X == 0 && CellOffset.Y == 0 && CellOffset.Z == 0) continue;

		// Shift grid data (opposite direction so wind stays in world space)
		Level.Solver->ShiftData(
			FIntVector(-CellOffset.X, -CellOffset.Y, -CellOffset.Z),
			AmbientWind);

		// Move grid origin
		const FVector OriginShift(CellOffset.X * CS, CellOffset.Y * CS, CellOffset.Z * CS);
		Level.Solver->SetWorldOrigin(Level.Solver->GetWorldOrigin() + OriginShift);

		// Consume the cell-snapped portion
		Level.AccumulatedDelta -= OriginShift;
	}
}

// ---- Sampling ----

float FWindCascade::ComputeBlendFactor(int32 LevelIndex, const FVector& WorldPos) const
{
	const FWindCascadeLevel& Level = Levels[LevelIndex];
	const IWindSolver& S = *Level.Solver;

	const FVector GridMin = S.GetWorldOrigin();
	const FVector GridMax = GridMin + FVector(
		S.GetSizeX() * S.GetCellSize(),
		S.GetSizeY() * S.GetCellSize(),
		S.GetSizeZ() * S.GetCellSize());

	// Check if outside grid entirely
	if (WorldPos.X < GridMin.X || WorldPos.X > GridMax.X ||
	    WorldPos.Y < GridMin.Y || WorldPos.Y > GridMax.Y ||
	    WorldPos.Z < GridMin.Z || WorldPos.Z > GridMax.Z)
	{
		return 0.f;
	}

	// Distance from each edge in world units
	const float BlendDist = Level.Config.BlendCells * Level.Config.CellSize;
	if (BlendDist <= 0.f) return 1.f;

	// Minimum distance to any edge
	const float DxMin = FMath::Min(WorldPos.X - GridMin.X, GridMax.X - WorldPos.X);
	const float DyMin = FMath::Min(WorldPos.Y - GridMin.Y, GridMax.Y - WorldPos.Y);
	const float DzMin = FMath::Min(WorldPos.Z - GridMin.Z, GridMax.Z - WorldPos.Z);
	const float MinEdgeDist = FMath::Min3(DxMin, DyMin, DzMin);

	if (MinEdgeDist >= BlendDist) return 1.f;

	// Smooth blend from 0 at edge to 1 at BlendDist inward
	return FMath::SmoothStep(0.f, 1.f, MinEdgeDist / BlendDist);
}

FVector FWindCascade::SampleVelocityAt(const FVector& WorldPos) const
{
	if (Levels.Num() == 0) return FVector::ZeroVector;

	// Single cascade — fast path
	if (Levels.Num() == 1)
	{
		return Levels[0].Solver->SampleVelocityAt(WorldPos);
	}

	// Try finest cascade first, blend with next coarser at boundaries
	for (int32 i = 0; i < Levels.Num(); i++)
	{
		const float Blend = ComputeBlendFactor(i, WorldPos);

		if (Blend <= 0.f) continue; // Outside this cascade

		if (Blend >= 1.f)
		{
			// Fully inside this cascade — use it directly
			return Levels[i].Solver->SampleVelocityAt(WorldPos);
		}

		// In the blend zone: interpolate with next coarser cascade
		const FVector FineVel = Levels[i].Solver->SampleVelocityAt(WorldPos);

		// Find next coarser cascade that contains this point
		for (int32 j = i + 1; j < Levels.Num(); j++)
		{
			const float CoarseBlend = ComputeBlendFactor(j, WorldPos);
			if (CoarseBlend > 0.f)
			{
				const FVector CoarseVel = Levels[j].Solver->SampleVelocityAt(WorldPos);
				return FMath::Lerp(CoarseVel, FineVel, Blend);
			}
		}

		// No coarser cascade covers this point — use fine with partial blend
		return FineVel * Blend;
	}

	return FVector::ZeroVector;
}

float FWindCascade::SampleTurbulenceAt(const FVector& WorldPos) const
{
	if (Levels.Num() == 0) return 0.f;

	if (Levels.Num() == 1)
	{
		return Levels[0].Solver->SampleTurbulenceAt(WorldPos);
	}

	for (int32 i = 0; i < Levels.Num(); i++)
	{
		const float Blend = ComputeBlendFactor(i, WorldPos);

		if (Blend <= 0.f) continue;

		if (Blend >= 1.f)
		{
			return Levels[i].Solver->SampleTurbulenceAt(WorldPos);
		}

		const float FineTurb = Levels[i].Solver->SampleTurbulenceAt(WorldPos);

		for (int32 j = i + 1; j < Levels.Num(); j++)
		{
			const float CoarseBlend = ComputeBlendFactor(j, WorldPos);
			if (CoarseBlend > 0.f)
			{
				const float CoarseTurb = Levels[j].Solver->SampleTurbulenceAt(WorldPos);
				return FMath::Lerp(CoarseTurb, FineTurb, Blend);
			}
		}

		return FineTurb * Blend;
	}

	return 0.f;
}

// ---- Obstacles ----

void FWindCascade::ClearSolids()
{
	for (auto& Level : Levels)
	{
		Level.Solver->ClearSolids();
	}
}

void FWindCascade::MarkSolid(int32 LevelIndex, int32 X, int32 Y, int32 Z)
{
	if (Levels.IsValidIndex(LevelIndex))
	{
		Levels[LevelIndex].Solver->MarkSolid(X, Y, Z);
	}
}

// ---- Default Configs ----

TArray<FWindCascadeConfig> FWindCascade::GetDefaultConfigs()
{
	TArray<FWindCascadeConfig> Configs;

	// Level 0: Fine — near player, high detail (grass/foliage response)
	FWindCascadeConfig Fine;
	Fine.CellSize = 100.f;
	Fine.SizeX = 32;
	Fine.SizeY = 32;
	Fine.SizeZ = 8;
	Fine.BlendCells = 4;
	Configs.Add(Fine);

	// Level 1: Medium — mid-range coverage
	FWindCascadeConfig Medium;
	Medium.CellSize = 200.f;
	Medium.SizeX = 24;
	Medium.SizeY = 24;
	Medium.SizeZ = 8;
	Medium.BlendCells = 3;
	Configs.Add(Medium);

	// Level 2: Coarse — distant coverage (trees, large vegetation)
	FWindCascadeConfig Coarse;
	Coarse.CellSize = 400.f;
	Coarse.SizeX = 16;
	Coarse.SizeY = 16;
	Coarse.SizeZ = 8;
	Coarse.BlendCells = 2;
	Configs.Add(Coarse);

	return Configs;
}
