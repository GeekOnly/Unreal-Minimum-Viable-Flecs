#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WindGrid.h"
#include "WindTypes.h"
#include "WindTextureManager.h"
#include "flecs.h"
#include "WindSubsystem.generated.h"

USTRUCT(BlueprintType)
struct WIND3DINTERACTIVE_API FWindEntityHandle
{
	GENERATED_BODY()

	FWindEntityHandle() {}
	FWindEntityHandle(int64 InId) : EntityId(InId) {}

	UPROPERTY(BlueprintReadOnly, Category = "Wind3D")
	int64 EntityId = 0;

	bool IsValid() const { return EntityId != 0; }
};

UCLASS()
class WIND3DINTERACTIVE_API UWindSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- UTickableWorldSubsystem Interface ---
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// -----------------------------------------

	// --- Blueprint API ---

	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	FWindEntityHandle RegisterWindMotor(
		FVector Position,
		FVector Direction,
		float Strength,
		float Radius,
		EWindMotorShape Shape,
		EWindEmissionType EmissionType);

	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	void UpdateWindMotor(
		FWindEntityHandle Handle,
		FVector Position,
		FVector Direction,
		float Strength,
		float Radius,
		EWindMotorShape Shape,
		EWindEmissionType EmissionType,
		float Height = 0.f,
		float InnerRadius = 0.f,
		float VortexAngularSpeed = 0.f,
		bool bEnabled = true,
		float TopRadius = 0.f,
		float MoveLength = 0.f,
		float ImpulseScale = 1.f,
		FVector AngularVelocity = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	void UnregisterWindMotor(FWindEntityHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	FWindEntityHandle RegisterFoliageInstance(
		UHierarchicalInstancedStaticMeshComponent* HISM,
		int32 InstanceIndex,
		FVector WorldLocation,
		float Sensitivity = 1.f,
		float Stiffness = 10.f,
		float Damping = 2.f,
		int32 CPDSlotDisplace = 0,
		int32 CPDSlotTurbulence = 1);

	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	FVector QueryWindAtPosition(FVector WorldPosition) const;

	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	void SetupWindGrid(FVector Origin, float CellSize, int32 SizeX, int32 SizeY, int32 SizeZ);

	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	void SetAmbientWind(FVector AmbientVelocity);

	/** Optional: set an actor for the grid to follow (e.g. player pawn). Grid re-centers when it moves. */
	UFUNCTION(BlueprintCallable, Category = "Wind3D")
	void SetGridCenterActor(AActor* Actor);

	// --- Material Integration ---

	/** Get the wind atlas texture for binding to materials. */
	UFUNCTION(BlueprintPure, Category = "Wind3D|Material")
	UTexture2D* GetWindAtlasTexture() const;

	/** Get the wind Material Parameter Collection for referencing in materials. */
	UFUNCTION(BlueprintPure, Category = "Wind3D|Material")
	UMaterialParameterCollection* GetWindMPC() const;

	/** Convenience: bind wind atlas texture to a dynamic material instance. */
	UFUNCTION(BlueprintCallable, Category = "Wind3D|Material")
	void BindWindToMaterial(UMaterialInstanceDynamic* MID, FName TextureParamName = "WindAtlas");

	// --- C++ access ---
	flecs::world* GetEcsWorld() const { return ECSWorld; }
	const FWindGrid& GetWindGrid() const { return WindGrid; }
	FWindGrid& GetWindGridMutable() { return WindGrid; }

	// --- Simulation Parameters ---
	float DiffusionRate = 0.15f;
	float AdvectionForce = 1.0f;   // 1.0 = physically correct (velocity in cm/s / CellSize)
	float DecayRate = 2.f;
	int32 DiffusionIterations = 2;
	bool bForwardAdvection = true;
	int32 BoundaryFadeCells = 2;
	int32 PressureIterations = 20; // Gauss-Seidel iterations for divergence removal

	// --- Material Integration Parameters ---
	float OverallPower = 1.0f;
	bool bEnableMaterialIntegration = true;

	// --- Obstacle Detection Parameters ---
	bool bEnableObstacles = false;
	int32 ObstacleUpdateInterval = 5;
	TEnumAsByte<ECollisionChannel> ObstacleChannel = ECC_WorldStatic;

	// --- World Wind Parameters ---
	bool bEnableWorldWind = false;
	float WorldWindSpeed = 200.f;
	float WorldWindRotationSpeed = 15.f;
	float WorldWindNoiseAmplitude = 30.f;
	float WorldWindNoiseFrequency = 0.3f;
	float WorldWindSpeedNoiseAmplitude = 50.f;

protected:
	flecs::world* ECSWorld = nullptr;
	FWindGrid WindGrid;
	FVector AmbientWind = FVector(100.f, 0.f, 0.f);

private:
	void RegisterComponents();
	void RegisterSystems();
	void DrawDebugWind();
	void UpdateGridOffset();
	void UpdateWorldWind(float DeltaTime);
	void UpdateOccupancy();

	// World wind state
	float WorldWindTime = 0.f;

	// Obstacle detection state
	int32 ObstacleFrameCounter = 0;

	// Grid-follow state
	TWeakObjectPtr<AActor> GridCenterActor;
	FVector PreviousGridCenter = FVector::ZeroVector;
	bool bGridCenterInitialized = false;

	// Track HISM components that need MarkRenderStateDirty after ECS progress
	TSet<UHierarchicalInstancedStaticMeshComponent*> DirtyHISMs;

	// Motor entity pool — recycled on unregister, reused on register
	TArray<flecs::entity> MotorPool;

	// Material integration
	FWindTextureManager TextureManager;
	bool bTextureManagerInitialized = false;
};
