#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"

class UVolumeTexture;
class UTexture2D;
class UMaterialParameterCollection;
class UMaterialInstanceDynamic;
class UWorld;
struct IWindSolver;

/**
 * Manages the wind 3D volume texture and Material Parameter Collection.
 * Owned by UWindSubsystem. Not a UObject — plain C++ class.
 *
 * Each frame after simulation: encodes IWindSolver velocities into a staging buffer,
 * uploads to GPU as a UVolumeTexture, and updates MPC parameters.
 */
class WIND3DINTERACTIVE_API FWindTextureManager
{
public:
	FWindTextureManager();
	~FWindTextureManager();

	/** Call once after grid dimensions are known. Creates volume texture + MPC. */
	void Initialize(UWorld* World, int32 SizeX, int32 SizeY, int32 SizeZ);

	/** Call on subsystem shutdown. Releases texture + MPC. */
	void Shutdown();

	/**
	 * Call each frame after simulation completes.
	 * Encodes grid data → staging buffer → GPU upload + MPC update.
	 */
	void UpdateFromGrid(
		const IWindSolver& Grid,
		const FVector& AmbientWind,
		float OverallPower);

	/** The 3D volume texture for material binding. */
	UVolumeTexture* GetWindVolumeTexture() const { return WindVolumeTexture; }

	/** 2D top-down slice texture for UI visualization (PF_B8G8R8A8).
	 *  R = speed magnitude, G = X velocity direction, B = Y velocity direction, A = 255.
	 *  Set VizZSlice before UpdateFromGrid to choose which Z layer to show (-1 = center). */
	UTexture2D* GetWindVizTexture() const { return WindVizTexture; }

	/** Which Z slice to visualize. -1 = auto (center slice). */
	void SetVizZSlice(int32 ZSlice) { VizZSlice = ZSlice; }

	/** The MPC for material parameter reference. */
	UMaterialParameterCollection* GetWindMPC() const { return WindMPC; }

	/** Convenience: set wind volume texture on a dynamic material instance. */
	void BindToMaterial(UMaterialInstanceDynamic* MID, FName TextureParamName);

	/** Update VolumeOrigin/VolumeSize on all bound MIDs. Called each frame from UpdateFromGrid. */
	void UpdateBoundMIDs(const IWindSolver& Grid);

	bool IsInitialized() const { return bInitialized; }

	/** Max wind speed for encoding (must match WindSampling.usf). */
	static constexpr float MaxWindSpeed = 2000.0f;

private:
	void CreateVolumeTexture();
	void CreateMPC();
	void CreateVizTexture();
	void EncodeGridToStagingBuffer(const IWindSolver& Grid);
	void UploadToGPU();
	void UpdateMPCParams(const IWindSolver& Grid, const FVector& AmbientWind, float OverallPower);
	void UpdateVizSlice(const IWindSolver& Grid);

	// Cached grid dimensions
	int32 GridSizeX = 0;
	int32 GridSizeY = 0;
	int32 GridSizeZ = 0;

	// Staging buffer (game thread) — flat 3D: index = Z * SizeX * SizeY + Y * SizeX + X
	TArray<FFloat16Color> StagingBuffer;

	// 2D viz slice staging buffer (BGRA, game thread)
	TArray<uint8> VizStagingBuffer;
	int32 VizZSlice = -1; // -1 = auto center

	// UE objects (rooted to prevent GC)
	UVolumeTexture* WindVolumeTexture = nullptr;
	UTexture2D* WindVizTexture = nullptr;
	UMaterialParameterCollection* WindMPC = nullptr;

	TWeakObjectPtr<UWorld> CachedWorld;
	bool bInitialized = false;

	// Tracked MIDs for per-frame VolumeOrigin/VolumeSize updates
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> BoundMIDs;
};
