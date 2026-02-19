#pragma once

#include "CoreMinimal.h"
#include "IWindSolver.h"

/**
 * GPU implementation of the wind solver using Compute Shaders.
 * Stores velocity field in RWTexture3D<float4> for direct material sampling.
 * Uses async readback for CPU-side queries (1-frame latency).
 *
 * TODO: This is a skeleton — compute shader dispatch will be implemented
 * once the module split is verified working with CPU path.
 */
struct WIND3DINTERACTIVEGPU_API FWindGridGPU : public IWindSolver
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

	// ---- Query (GPU readback — 1-frame latency) ----
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

	// ---- Buffer Access (CPU readback copy) ----
	virtual const TArray<FVector>& GetVelocities() const override { return ReadbackVelocities; }
	virtual const TArray<float>& GetTurbulences() const override { return ReadbackTurbulences; }

	// ---- Coordinate Helpers ----
	virtual int32 WorldToIndex(const FVector& WorldPos) const override;
	virtual FIntVector WorldToCell(const FVector& WorldPos) const override;
	virtual FVector CellToWorld(int32 X, int32 Y, int32 Z) const override;
	virtual int32 CellIndex(int32 X, int32 Y, int32 Z) const override;
	virtual bool IsInBounds(int32 X, int32 Y, int32 Z) const override;

private:
	int32 SizeX = 16;
	int32 SizeY = 16;
	int32 SizeZ = 8;
	float CellSize = 200.f;
	FVector WorldOrigin = FVector::ZeroVector;

	// CPU-side readback buffers (updated at end of frame via async readback)
	TArray<FVector> ReadbackVelocities;
	TArray<float>   ReadbackTurbulences;
	TArray<uint8>   Solids; // CPU-side for MarkSolid/IsSolid, uploaded to GPU per frame

	// TODO: GPU resources
	// FTexture3DRHIRef VelocityTexA;  // Ping
	// FTexture3DRHIRef VelocityTexB;  // Pong
	// FTexture3DRHIRef SolidsTexture; // Uploaded from CPU
	// FStructuredBufferRHIRef MotorBuffer;
};
