#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindFieldSetupActor.generated.h"

class UBoxComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UWindSubsystem;

UENUM(BlueprintType)
enum class EWindFoliagePhysicsPreset : uint8
{
	Custom UMETA(DisplayName = "Custom"),
	Grass  UMETA(DisplayName = "Grass"),
	Shrub  UMETA(DisplayName = "Shrub"),
	Tree   UMETA(DisplayName = "Tree")
};

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

	// --- Foliage Auto Registration ---

	/** Automatically register HISM foliage instances into WindSubsystem on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Auto Register")
	bool bAutoRegisterFoliage = true;

	/** One-control preset for foliage physics tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Preset", meta = (EditCondition = "bAutoRegisterFoliage"))
	EWindFoliagePhysicsPreset FoliagePhysicsPreset = EWindFoliagePhysicsPreset::Shrub;

	/** Apply selected preset values into editable default fields (Call in Editor button). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Foliage|Preset")
	void ApplyFoliagePresetToDefaults();

	/** Optional source actors. Empty = scan all actors in world for HISM components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Auto Register", meta = (EditCondition = "bAutoRegisterFoliage"))
	TArray<TSoftObjectPtr<AActor>> FoliageSourceActors;

	/** Register only static HISM components. Recommended for painted foliage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Auto Register", meta = (EditCondition = "bAutoRegisterFoliage"))
	bool bRegisterStaticOnly = true;

	/** Optional component tag filter. None = include all HISM components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Auto Register", meta = (EditCondition = "bAutoRegisterFoliage"))
	FName RequiredFoliageComponentTag = NAME_None;

	/** Only register instances within this radius from setup actor. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Auto Register", meta = (ClampMin = "0.0", EditCondition = "bAutoRegisterFoliage"))
	float AutoRegisterRadius = 0.f;

	/** Hard cap to avoid expensive accidental full-map registration. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Auto Register", meta = (ClampMin = "0", EditCondition = "bAutoRegisterFoliage"))
	int32 MaxAutoRegisterInstances = 30000;

	/** Custom primitive data slot for displacement output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|CPD", meta = (ClampMin = "0", EditCondition = "bAutoRegisterFoliage"))
	int32 FoliageCPDSlotDisplace = 0;

	/** Custom primitive data slot for turbulence output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|CPD", meta = (ClampMin = "0", EditCondition = "bAutoRegisterFoliage"))
	int32 FoliageCPDSlotTurbulence = 1;

	// --- Foliage Physics Defaults ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.0", EditCondition = "bAutoRegisterFoliage"))
	float FoliageSensitivity = 1.f;

	/** Legacy stiffness multiplier retained for existing authored behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.01", EditCondition = "bAutoRegisterFoliage"))
	float FoliageStiffness = 10.f;

	/** Legacy damping multiplier retained for existing authored behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.01", EditCondition = "bAutoRegisterFoliage"))
	float FoliageDamping = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.05", EditCondition = "bAutoRegisterFoliage"))
	float FoliageMass = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.1", EditCondition = "bAutoRegisterFoliage"))
	float FoliageSpringConstant = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.0", EditCondition = "bAutoRegisterFoliage"))
	float FoliageDampingCoefficient = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.05", EditCondition = "bAutoRegisterFoliage"))
	float FoliageMaxVelocity = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.1", EditCondition = "bAutoRegisterFoliage"))
	float FoliageMaxAcceleration = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.0", EditCondition = "bAutoRegisterFoliage"))
	float FoliageRestDisplacement = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage|Physics Defaults", meta = (ClampMin = "0.01", ClampMax = "1.0", EditCondition = "bAutoRegisterFoliage"))
	float FoliageWindFilterAlpha = 0.25f;

protected:
	virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> PreviewBox;

private:
    void UpdatePreviewBox();
	void AutoRegisterFoliage(UWindSubsystem* WindSys);
	bool ShouldRegisterFoliageComponent(const UHierarchicalInstancedStaticMeshComponent* HISM) const;
	bool IsInsideRegistrationRadius(const FVector& WorldLocation) const;
	void ResolveFoliagePhysicsDefaults(
		float& OutSensitivity,
		float& OutStiffness,
		float& OutDamping,
		float& OutMass,
		float& OutSpringConstant,
		float& OutDampingCoefficient,
		float& OutMaxVelocity,
		float& OutMaxAcceleration,
		float& OutRestDisplacement,
		float& OutWindFilterAlpha) const;
};
