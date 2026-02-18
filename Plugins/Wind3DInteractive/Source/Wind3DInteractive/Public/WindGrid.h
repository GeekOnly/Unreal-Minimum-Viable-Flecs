#pragma once

#include "CoreMinimal.h"

struct FWindMotorData;

/**
 * CPU-side 3D array of wind vectors.
 * Grid origin, cell size, and dimensions define world-space coverage.
 */
struct WIND3DINTERACTIVE_API FWindGrid
{
	int32 SizeX = 16;
	int32 SizeY = 16;
	int32 SizeZ = 8;

	FVector WorldOrigin = FVector::ZeroVector;
	float   CellSize = 200.f; // Unreal units (cm)

	TArray<FVector> Velocities;
	TArray<FVector> VelocitiesBack; // back buffer for ping-pong
	TArray<float>   Turbulences;
	TArray<uint8>   Solids;         // 1 = solid (blocked by collision), 0 = empty

	void Initialize();
	void Reset();

	int32 WorldToIndex(const FVector& WorldPos) const;
	FIntVector WorldToCell(const FVector& WorldPos) const;
	FVector CellToWorld(int32 X, int32 Y, int32 Z) const;

	FVector SampleVelocityAt(const FVector& WorldPos) const;
	FVector SampleVelocityAtLocal(float Lx, float Ly, float Lz) const;
	float   SampleTurbulenceAt(const FVector& WorldPos) const;

	void InjectMotor(const FWindMotorData& Motor, float DeltaTime);
	void DecayToAmbient(FVector AmbientWind, float DecayRate, float DeltaTime);

	// GoW simulation pipeline
	void SwapBuffers();
	void Diffuse(float DiffusionRate, float DeltaTime, int32 Iterations = 1);
	void Advect(float AdvectionForce, float DeltaTime, bool bForwardPass = true);
	void BoundaryFadeOut(int32 FadeCells = 2);
	void ShiftData(FIntVector CellOffset, FVector AmbientWind);

	int32 GetTotalCells() const { return SizeX * SizeY * SizeZ; }

	// Occupancy grid (obstacle detection)
	void ClearSolids();
	void MarkSolid(int32 X, int32 Y, int32 Z);
	bool IsSolid(int32 X, int32 Y, int32 Z) const;

	int32 CellIndex(int32 X, int32 Y, int32 Z) const;
	bool  IsInBounds(int32 X, int32 Y, int32 Z) const;
};
