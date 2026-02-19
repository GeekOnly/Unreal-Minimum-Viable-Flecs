#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class UMaterialParameterCollection;
class UMaterialInstanceDynamic;
class UWorld;
struct IWindSolver;

/**
 * Manages the wind data texture (2D atlas of Z slices) and Material Parameter Collection.
 * Owned by UWindSubsystem. Not a UObject — plain C++ class.
 *
 * Each frame after simulation: encodes IWindSolver velocities into a staging buffer,
 * uploads to GPU via render command, and updates MPC parameters.
 */
class WIND3DINTERACTIVE_API FWindTextureManager
{
public:
	FWindTextureManager();
	~FWindTextureManager();

	/** Call once after grid dimensions are known. Creates texture + MPC. */
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

	/** The atlas texture for material binding. */
	UTexture2D* GetWindAtlasTexture() const { return WindAtlasTexture; }

	/** The MPC for material parameter reference. */
	UMaterialParameterCollection* GetWindMPC() const { return WindMPC; }

	/** Convenience: set wind atlas texture on a dynamic material instance. */
	void BindToMaterial(UMaterialInstanceDynamic* MID, FName TextureParamName) const;

	bool IsInitialized() const { return bInitialized; }

	/** Max wind speed for encoding (must match WindSampling.usf). */
	static constexpr float MaxWindSpeed = 2000.0f;

private:
	void CreateAtlasTexture();
	void CreateMPC();
	void EncodeGridToStagingBuffer(const IWindSolver& Grid);
	void UploadToGPU();
	void UpdateMPCParams(const IWindSolver& Grid, const FVector& AmbientWind, float OverallPower);

	// Cached grid dimensions
	int32 GridSizeX = 0;
	int32 GridSizeY = 0;
	int32 GridSizeZ = 0;

	// Atlas dimensions (pixels)
	int32 AtlasWidth = 0;   // SizeX * SizeZ
	int32 AtlasHeight = 0;  // SizeY

	// Staging buffer (game thread)
	TArray<FFloat16Color> StagingBuffer;

	// UE objects (rooted to prevent GC)
	UTexture2D* WindAtlasTexture = nullptr;
	UMaterialParameterCollection* WindMPC = nullptr;

	TWeakObjectPtr<UWorld> CachedWorld;
	bool bInitialized = false;
};
