#pragma once

#include "CoreMinimal.h"
#include "IWindSolver.h"

/**
 * CPU implementation of the wind solver.
 * Uses ParallelFor for hot loops (Diffuse, Advect, Pressure).
 * Optimized with SIMD-friendly data layout.
 * This is the same math as the original FWindGrid, refactored behind IWindSolver.
 */
struct WIND3DINTERACTIVECPU_API FWindGridCPU : public IWindSolver
{
	// ---- Setup ----
	virtual void Initialize(int32 InSizeX, int32 InSizeY, int32 InSizeZ, float InCellSize) override;
	virtual void Reset() override;

	// ---- Simulation Pipeline ----
	virtual void InjectMotor(const FWindMotorData& Motor, float DeltaTime) override;
	virtual void DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime) override;
	virtual void Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations = 1) override;
	virtual void Advect(float AdvectionForce, float DeltaTime, bool bForwardPass = true) override;
	virtual void ProjectPressure(int32 Iterations = 20) override;
	virtual void BoundaryFadeOut(int32 FadeCells = 2) override;
	virtual void ShiftData(FIntVector CellOffset, FVector AmbientWind) override;

	// ---- Query ----
	virtual FVector SampleVelocityAt(const FVector& WorldPos) const override;
	virtual FVector SampleVelocityAtLocal(float Lx, float Ly, float Lz) const override;
	virtual float SampleTurbulenceAt(const FVector& WorldPos) const override;

	// ---- Obstacle Grid ----
	virtual void ClearSolids() override;
	virtual void MarkSolid(int32 X, int32 Y, int32 Z) override;
	virtual bool IsSolid(int32 X, int32 Y, int32 Z) const override;

	// ---- Grid Info ----
	virtual int32 GetSizeX() const override { return SizeX; }
	virtual int32 GetSizeY() const override { return SizeY; }
	virtual int32 GetSizeZ() const override { return SizeZ; }
	virtual float GetCellSize() const override { return CellSize; }
	virtual FVector GetWorldOrigin() const override { return WorldOrigin; }
	virtual void SetWorldOrigin(const FVector& Origin) override { WorldOrigin = Origin; }
	virtual int32 GetTotalCells() const override { return SizeX * SizeY * SizeZ; }

	// ---- Buffer Access ----
	virtual const TArray<FVector>& GetVelocities() const override { return Velocities; }
	virtual const TArray<float>& GetTurbulences() const override { return Turbulences; }

	// ---- Coordinate Helpers ----
	virtual int32 WorldToIndex(const FVector& WorldPos) const override;
	virtual FIntVector WorldToCell(const FVector& WorldPos) const override;
	virtual FVector CellToWorld(int32 X, int32 Y, int32 Z) const override;
	virtual int32 CellIndex(int32 X, int32 Y, int32 Z) const override;
	virtual bool IsInBounds(int32 X, int32 Y, int32 Z) const override;

private:
	void SwapBuffers();

	int32 SizeX = 16;
	int32 SizeY = 16;
	int32 SizeZ = 8;
	float CellSize = 200.f;
	FVector WorldOrigin = FVector::ZeroVector;

	TArray<FVector> Velocities;
	TArray<FVector> VelocitiesBack;
	TArray<float>   Turbulences;
	TArray<uint8>   Solids;
	TArray<float>   AdvectWeights; // Reused per-frame (avoids allocation in Advect)

	/** Minimum cells per batch for ParallelFor (avoids overhead on small grids). */
	static constexpr int32 ParallelMinBatch = 256;
};
