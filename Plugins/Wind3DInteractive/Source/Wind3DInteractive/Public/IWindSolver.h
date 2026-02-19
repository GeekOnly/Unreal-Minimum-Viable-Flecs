#pragma once

#include "CoreMinimal.h"

struct FWindMotorData;

/**
 * Abstract solver interface for 3D wind simulation.
 * Implementations: FWindGridCPU (CPU), FWindGridGPU (Compute Shader).
 * WindSubsystem owns one instance and calls the pipeline each frame.
 */
struct WIND3DINTERACTIVE_API IWindSolver
{
	virtual ~IWindSolver() = default;

	// ---- Setup ----
	virtual void Initialize(int32 InSizeX, int32 InSizeY, int32 InSizeZ, float InCellSize) = 0;
	virtual void Reset() = 0;

	// ---- Simulation Pipeline (order matters) ----

	/** Inject a single motor's contribution into the grid. */
	virtual void InjectMotor(const FWindMotorData& Motor, float DeltaTime) = 0;

	/** Decay velocities toward ambient wind. */
	virtual void DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime) = 0;

	/** Diffuse velocities (3D blur). */
	virtual void Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations = 1) = 0;

	/** Advect velocities (self-transport). */
	virtual void Advect(float AdvectionForce, float DeltaTime, bool bForwardPass = true) = 0;

	/** Project pressure to enforce divergence-free flow. */
	virtual void ProjectPressure(int32 Iterations = 20) = 0;

	/** Fade velocities at grid boundaries. */
	virtual void BoundaryFadeOut(int32 FadeCells = 2) = 0;

	/** Shift grid data when the grid origin moves. */
	virtual void ShiftData(FIntVector CellOffset, FVector AmbientWind) = 0;

	// ---- Query (CPU-side read) ----

	/** Sample interpolated wind velocity at world position. */
	virtual FVector SampleVelocityAt(const FVector& WorldPos) const = 0;

	/** Sample velocity in local grid coordinates. */
	virtual FVector SampleVelocityAtLocal(float Lx, float Ly, float Lz) const = 0;

	/** Sample turbulence at world position. */
	virtual float SampleTurbulenceAt(const FVector& WorldPos) const = 0;

	// ---- Obstacle Grid ----
	virtual void ClearSolids() = 0;
	virtual void MarkSolid(int32 X, int32 Y, int32 Z) = 0;
	virtual bool IsSolid(int32 X, int32 Y, int32 Z) const = 0;

	// ---- Grid Info ----
	virtual int32 GetSizeX() const = 0;
	virtual int32 GetSizeY() const = 0;
	virtual int32 GetSizeZ() const = 0;
	virtual float GetCellSize() const = 0;
	virtual FVector GetWorldOrigin() const = 0;
	virtual void SetWorldOrigin(const FVector& Origin) = 0;
	virtual int32 GetTotalCells() const = 0;

	// ---- Buffer Access (for TextureManager / Debug viz) ----
	virtual const TArray<FVector>& GetVelocities() const = 0;
	virtual const TArray<float>& GetTurbulences() const = 0;

	// ---- Coordinate Helpers ----
	virtual int32 WorldToIndex(const FVector& WorldPos) const = 0;
	virtual FIntVector WorldToCell(const FVector& WorldPos) const = 0;
	virtual FVector CellToWorld(int32 X, int32 Y, int32 Z) const = 0;
	virtual int32 CellIndex(int32 X, int32 Y, int32 Z) const = 0;
	virtual bool IsInBounds(int32 X, int32 Y, int32 Z) const = 0;
};

/**
 * Static factory for creating solver instances.
 * CPU/GPU modules register their factory delegates at module startup.
 * This breaks the circular dependency: Core never includes CPU/GPU headers.
 */
DECLARE_DELEGATE_RetVal(TUniquePtr<IWindSolver>, FWindSolverCreateDelegate);

struct WIND3DINTERACTIVE_API FWindSolverFactory
{
	static FWindSolverCreateDelegate CreateCPU;
	static FWindSolverCreateDelegate CreateGPU;
};
