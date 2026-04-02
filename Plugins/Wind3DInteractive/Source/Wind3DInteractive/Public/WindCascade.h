#pragma once

#include "CoreMinimal.h"
#include "IWindSolver.h"

struct FWindMotorData;

/**
 * Configuration for a single cascade level.
 * Smaller CellSize = finer detail but smaller coverage.
 */
struct WIND3DINTERACTIVE_API FWindCascadeConfig
{
	float CellSize = 200.f;
	int32 SizeX = 16;
	int32 SizeY = 16;
	int32 SizeZ = 8;

	/** Number of cells at the edge used for blending with the next coarser level. */
	int32 BlendCells = 3;

	FVector GetCoverage() const
	{
		return FVector(SizeX * CellSize, SizeY * CellSize, SizeZ * CellSize);
	}
};

/**
 * A single cascade level: owns a solver and tracks per-level grid offset state.
 */
struct WIND3DINTERACTIVE_API FWindCascadeLevel
{
	TUniquePtr<IWindSolver> Solver;
	FWindCascadeConfig Config;

	// Per-level grid offset tracking (different cell sizes = different shift granularity)
	FVector AccumulatedDelta = FVector::ZeroVector;

	// Move-only (TUniquePtr is non-copyable)
	FWindCascadeLevel() = default;
	FWindCascadeLevel(FWindCascadeLevel&&) = default;
	FWindCascadeLevel& operator=(FWindCascadeLevel&&) = default;
	FWindCascadeLevel(const FWindCascadeLevel&) = delete;
	FWindCascadeLevel& operator=(const FWindCascadeLevel&) = delete;
};

/**
 * Cascaded wind grid system.
 *
 * Multiple overlapping grids centered on the player, each at a different resolution:
 *   Level 0 (finest):  small cells, highest detail near the player
 *   Level 1 (medium):  medium cells, wider coverage
 *   Level N (coarsest): large cells, widest coverage
 *
 * Motors inject into ALL levels they overlap.
 * Sampling picks the finest level that contains the query point, with smooth
 * blending at cascade boundaries to prevent popping.
 *
 * All grids follow the same center actor (player).
 */
class WIND3DINTERACTIVE_API FWindCascade
{
public:
	FWindCascade();
	~FWindCascade();

	// Move-only (contains TArray of move-only levels)
	FWindCascade(FWindCascade&&) = default;
	FWindCascade& operator=(FWindCascade&&) = default;
	FWindCascade(const FWindCascade&) = delete;
	FWindCascade& operator=(const FWindCascade&) = delete;

	/**
	 * Initialize with cascade configs (ordered finest to coarsest).
	 * Creates one IWindSolver per level via the factory.
	 */
	void Initialize(const TArray<FWindCascadeConfig>& Configs, bool bUseGPU = false);

	/** Shut down all levels. */
	void Shutdown();

	bool IsInitialized() const { return Levels.Num() > 0; }
	int32 GetNumLevels() const { return Levels.Num(); }

	/** Access a specific cascade level. */
	FWindCascadeLevel& GetLevel(int32 Index) { return Levels[Index]; }
	const FWindCascadeLevel& GetLevel(int32 Index) const { return Levels[Index]; }

	/** The finest (level 0) solver — used for texture/material integration. */
	IWindSolver& GetFinestSolver() { check(Levels.Num() > 0); return *Levels[0].Solver; }
	const IWindSolver& GetFinestSolver() const { check(Levels.Num() > 0); return *Levels[0].Solver; }

	// ---- Simulation Pipeline (dispatched to all levels) ----

	/** Inject a motor into all cascade levels it overlaps. */
	void InjectMotor(const FWindMotorData& Motor, float DeltaTime);

	void DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime);
	void Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations);
	void Advect(float AdvectionForce, float DeltaTime, bool bForwardPass);
	void ProjectPressure(int32 Iterations);
	void BoundaryFadeOut(int32 FadeCells);

	// ---- Grid Following ----

	/** Set world origin for all cascades (centered). */
	void SetWorldOriginCentered(const FVector& CenterPos);

	/**
	 * Shift all grids when the center actor moves.
	 * Each level computes its own cell offset from the world-space delta.
	 */
	void ShiftAllGrids(const FVector& WorldDelta, const FVector& AmbientWind);

	// ---- Sampling (picks best cascade, blends at boundaries) ----

	/** Sample wind velocity at world position, using finest available cascade with edge blending. */
	FVector SampleVelocityAt(const FVector& WorldPos) const;

	/** Sample turbulence at world position. */
	float SampleTurbulenceAt(const FVector& WorldPos) const;

	// ---- Obstacle Grid ----
	void ClearSolids();
	void MarkSolid(int32 LevelIndex, int32 X, int32 Y, int32 Z);

	/** Get default cascade configs (3 levels: fine/medium/coarse). */
	static TArray<FWindCascadeConfig> GetDefaultConfigs();

private:
	/**
	 * Compute blend factor for a position within a cascade level.
	 * Returns 1.0 at center, fades to 0.0 at edges within BlendCells.
	 * If the position is outside the grid, returns 0.0.
	 */
	float ComputeBlendFactor(int32 LevelIndex, const FVector& WorldPos) const;

	/** Check if a motor's bounding sphere overlaps a cascade level's grid volume. */
	bool MotorOverlapsLevel(const FWindMotorData& Motor, int32 LevelIndex) const;

	TArray<FWindCascadeLevel> Levels;
};
