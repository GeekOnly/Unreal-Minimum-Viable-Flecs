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
	void Diffuse(float DiffusionRate, float DeltaTime);
	void Advect(float AdvectionForce, float DeltaTime);
	void ShiftData(FIntVector CellOffset, FVector AmbientWind);

	int32 GetTotalCells() const { return SizeX * SizeY * SizeZ; }

	int32 CellIndex(int32 X, int32 Y, int32 Z) const;
	bool  IsInBounds(int32 X, int32 Y, int32 Z) const;
};
