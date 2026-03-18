#pragma once

#include "CoreMinimal.h"
#include "IWindSolver.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "HAL/CriticalSection.h"
#include <atomic>

// ============================================================
// GPU-side motor data — must match FGPUMotorData in WindInjectMotor.usf exactly
// Layout: 96 bytes per motor
// ============================================================
// NOTE: FVector in UE5 is double-precision (24 bytes).
//       HLSL float3 = 12 bytes. We must use FVector3f (single-precision) here.
struct FGPUMotorData
{
	FVector3f WorldPosition;    float Strength;       // 12 + 4 = 16
	FVector3f Direction;        float Radius;         // 12 + 4 = 16
	FVector3f PreviousPosition; float Height;         // 12 + 4 = 16
	FVector3f AngularVelocity;  float VortexAngularSpeed; // 12 + 4 = 16
	float     InnerRadius;                            // 4
	float     TopRadius;                              // 4
	float     ImpulseScale;                           // 4
	float     MoveLength;                             // 4
	uint32    Shape;        // 0=Sphere, 1=Cylinder, 2=Cone  // 4
	uint32    EmissionType; // 0=Directional, 1=Omni, 2=Vortex, 3=Moving  // 4
	uint32    bEnabled;                               // 4
	uint32    _pad;                                   // 4
}; // Total: 4×16 + 4×4 + 4×4 = 64 + 16 + 16 = 96 bytes
static_assert(sizeof(FGPUMotorData) == 96, "FGPUMotorData size mismatch with HLSL struct");

/**
 * GPU implementation of the wind solver using Compute Shaders.
 *
 * Simulation pipeline (all dispatched via ENQUEUE_RENDER_COMMAND):
 *   DecayToAmbient → InjectMotors + Advect → Diffuse (N passes)
 *   → ProjectPressure (Jacobi N passes) → BoundaryFadeOut
 *
 * CPU queries (SampleVelocityAt, GetVelocities) use async readback buffers
 * with 1-frame latency — perfectly acceptable for wind simulation.
 *
 * Activate via console: Wind3D.UseGPU 1
 */
struct WIND3DINTERACTIVEGPU_API FWindGridGPU : public IWindSolver
{
	virtual ~FWindGridGPU();

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

	// ---- Query (uses CPU readback buffer — 1-frame latency) ----
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

	// ---- Buffer Access (CPU readback copy — 1-frame old) ----
	virtual const TArray<FVector>& GetVelocities() const override;
	virtual const TArray<float>& GetTurbulences() const override;

	// ---- Coordinate Helpers ----
	virtual int32 WorldToIndex(const FVector& WorldPos) const override;
	virtual FIntVector WorldToCell(const FVector& WorldPos) const override;
	virtual FVector CellToWorld(int32 X, int32 Y, int32 Z) const override;
	virtual int32 CellIndex(int32 X, int32 Y, int32 Z) const override;
	virtual bool IsInBounds(int32 X, int32 Y, int32 Z) const override;

private:
	// ---- Grid dimensions (set on game thread) ----
	int32   SizeX = 16;
	int32   SizeY = 16;
	int32   SizeZ = 8;
	float   CellSize = 200.f;
	FVector WorldOrigin = FVector::ZeroVector;

	// ---- GPU resources (created/destroyed on render thread) ----

	// Velocity ping-pong (float4: xyz=velocity cm/s, w=turbulence [0,1])
	FTextureRHIRef           VelocityTexA;
	FTextureRHIRef           VelocityTexB;
	FUnorderedAccessViewRHIRef VelocityUAV_A;
	FUnorderedAccessViewRHIRef VelocityUAV_B;
	FShaderResourceViewRHIRef  VelocitySRV_A;
	FShaderResourceViewRHIRef  VelocitySRV_B;
	bool bVelocityIsA = true;  // true → A is current, B is scratch

	// Pressure ping-pong (float single channel, for Jacobi iteration)
	FTextureRHIRef           PressureTexA;
	FTextureRHIRef           PressureTexB;
	FUnorderedAccessViewRHIRef PressureUAV_A;
	FUnorderedAccessViewRHIRef PressureUAV_B;
	FShaderResourceViewRHIRef  PressureSRV_A;
	FShaderResourceViewRHIRef  PressureSRV_B;
	bool bPressureIsA = true;

	// Solids texture (R32_UINT, written from CPU each simulation frame)
	FTextureRHIRef          SolidsTex;
	FShaderResourceViewRHIRef SolidsSRV;

	// Motor StructuredBuffer (dynamic, uploaded each frame)
	FBufferRHIRef             MotorBuffer;
	FShaderResourceViewRHIRef MotorSRV;
	static constexpr int32 MaxMotors = 256;

	// ---- CPU-side data ----

	// Obstacle grid (game thread, mirrored to GPU each frame)
	TArray<uint8>  Solids;
	TArray<uint32> SolidsGPUData;   // expanded from uint8, 4 bytes/cell

	// Motor accumulation (game thread → flushed during Advect)
	TArray<FGPUMotorData> PendingMotors;
	float PendingDeltaTime = 0.f;

	// ---- Async Readback (1-frame latency) ----
	TUniquePtr<FRHIGPUTextureReadback> VelocityReadback;
	std::atomic<bool> bReadbackPending{false};

	// Readback result buffers — written on render thread, read on game thread
	// Protected by ReadbackMutex
	mutable FCriticalSection   ReadbackMutex;
	TArray<FVector>            ReadbackVelocities;
	TArray<float>              ReadbackTurbulences;

	// Solids data — written on game thread (ClearSolids/MarkSolid), read for GPU upload
	// Protected by SolidsMutex
	mutable FCriticalSection   SolidsMutex;

	// PendingMotors — accumulated on game thread (InjectMotor), flushed in Advect
	// Protected by MotorMutex
	mutable FCriticalSection   MotorMutex;

	// Alive token — prevents dangling `this` in ENQUEUE_RENDER_COMMAND lambdas
	TSharedPtr<bool, ESPMode::ThreadSafe> AliveToken;

	bool bGPUResourcesCreated = false;

	// ---- Internal helpers ----
	FIntVector GetDispatchGroups() const
	{
		return FIntVector(
			FMath::DivideAndRoundUp(SizeX, 8),
			FMath::DivideAndRoundUp(SizeY, 8),
			FMath::DivideAndRoundUp(SizeZ, 4)
		);
	}

	// Current simulation texture getters (render thread only)
	FTextureRHIRef           GetCurrentTex()  const { return bVelocityIsA ? VelocityTexA : VelocityTexB; }
	FTextureRHIRef           GetScratchTex()  const { return bVelocityIsA ? VelocityTexB : VelocityTexA; }
	FUnorderedAccessViewRHIRef GetCurrentUAV()  const { return bVelocityIsA ? VelocityUAV_A : VelocityUAV_B; }
	FUnorderedAccessViewRHIRef GetScratchUAV()  const { return bVelocityIsA ? VelocityUAV_B : VelocityUAV_A; }
	FShaderResourceViewRHIRef  GetCurrentSRV()  const { return bVelocityIsA ? VelocitySRV_A : VelocitySRV_B; }
	FShaderResourceViewRHIRef  GetScratchSRV()  const { return bVelocityIsA ? VelocitySRV_B : VelocitySRV_A; }
	void SwapVelocityBuffers() { bVelocityIsA = !bVelocityIsA; }

	FUnorderedAccessViewRHIRef GetCurrentPressureUAV() const { return bPressureIsA ? PressureUAV_A : PressureUAV_B; }
	FUnorderedAccessViewRHIRef GetScratchPressureUAV()  const { return bPressureIsA ? PressureUAV_B : PressureUAV_A; }
	FShaderResourceViewRHIRef  GetCurrentPressureSRV() const { return bPressureIsA ? PressureSRV_A : PressureSRV_B; }
	FShaderResourceViewRHIRef  GetScratchPressureSRV()  const { return bPressureIsA ? PressureSRV_B : PressureSRV_A; }
	void SwapPressureBuffers() { bPressureIsA = !bPressureIsA; }

	void ReleaseGPUResources();
};
