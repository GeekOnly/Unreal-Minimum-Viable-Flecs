#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindFieldSetupActor.generated.h"

class UBoxComponent;

/**
 * Drag-and-drop actor to setup the Wind Grid volume and visualize it.
 */
UCLASS(ClassGroup = "Wind3D", meta = (DisplayName = "Wind Field Setup"))
class WIND3DINTERACTIVE_API AWindFieldSetupActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AWindFieldSetupActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "1"))
	int32 GridSizeX = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "1"))
	int32 GridSizeY = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "1"))
	int32 GridSizeZ = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid", meta = (ClampMin = "10.0"))
	float CellSize = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid")
	FVector AmbientWind = FVector(100.f, 0.f, 0.f);

	// --- World Wind (rotating ambient) ---

	/** Base wind speed (cm/s). Direction rotates over time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Wind", meta = (ClampMin = "0.0"))
	float WorldWindSpeed = 200.f;

	/** Rotation speed around Z-axis (degrees/sec). Creates swaying wind direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Wind")
	float WorldWindRotationSpeed = 15.f;

	/** Noise amplitude added to wind angle (degrees). Makes rotation organic, not perfectly circular. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Wind", meta = (ClampMin = "0.0"))
	float WorldWindNoiseAmplitude = 30.f;

	/** Noise frequency (cycles/sec). Lower = slower organic sway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Wind", meta = (ClampMin = "0.01"))
	float WorldWindNoiseFrequency = 0.3f;

	/** Noise amplitude for wind speed variation (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Wind", meta = (ClampMin = "0.0"))
	float WorldWindSpeedNoiseAmplitude = 50.f;

	/** Enable world wind (overrides static AmbientWind). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Wind")
	bool bEnableWorldWind = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bAutoStartDebug = true;

	// --- Material Integration ---

	/** Overall wind power multiplier applied in material shader. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Material", meta = (ClampMin = "0.0"))
	float OverallPower = 1.0f;

	/** Enable wind data output to materials (texture atlas + MPC updated each frame). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Material")
	bool bEnableMaterialIntegration = true;

	// --- Simulation Parameters ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DiffusionRate = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Simulation", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float AdvectionForce = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Simulation", meta = (ClampMin = "0.0"))
	float DecayRate = 2.f;

	/** Number of diffusion iterations per frame. More = smoother but costlier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Simulation", meta = (ClampMin = "1", ClampMax = "8"))
	int32 DiffusionIterations = 2;

	/** Enable forward advection pass before reverse (more accurate transport). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Simulation")
	bool bForwardAdvection = true;

	/** Number of cells from grid edge where wind fades to zero. 0 = disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Simulation", meta = (ClampMin = "0", ClampMax = "8"))
	int32 BoundaryFadeCells = 2;

	/** Gauss-Seidel iterations for pressure projection (divergence removal).
	 *  This makes wind flow AROUND obstacles. More = more accurate but costlier.
	 *  0 = disabled. 20 = good quality. 40 = high quality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Simulation", meta = (ClampMin = "0", ClampMax = "60"))
	int32 PressureIterations = 20;

	// --- Obstacle Detection ---

	/** Enable wind-obstacle collision (marks grid cells as solid via physics overlap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Obstacles")
	bool bEnableObstacles = false;

	/** How many frames between obstacle detection updates. Higher = cheaper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Obstacles", meta = (ClampMin = "1", ClampMax = "30", EditCondition = "bEnableObstacles"))
	int32 ObstacleUpdateInterval = 5;

	/** Collision channel used for obstacle detection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Obstacles", meta = (EditCondition = "bEnableObstacles"))
	TEnumAsByte<ECollisionChannel> ObstacleChannel = ECC_WorldStatic;

	// --- Cascade (Multi-Resolution) ---

	/** Enable cascaded multi-resolution grid. Finer detail near player, wider coverage at distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Cascade")
	bool bUseCascade = false;

	/** Number of cascade levels. Each level doubles the cell size and coverage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Cascade", meta = (ClampMin = "1", ClampMax = "4", EditCondition = "bUseCascade"))
	int32 NumCascadeLevels = 3;

	/** Optional: actor for the grid to follow (e.g. player pawn). Leave None for static grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Grid")
	TSoftObjectPtr<AActor> CenterActor;

protected:
	virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> PreviewBox;

private:
    void UpdatePreviewBox();
};
