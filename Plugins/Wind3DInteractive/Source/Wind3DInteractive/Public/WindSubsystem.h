#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WindGrid.h"
#include "WindTypes.h"
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
		bool bEnabled = true);

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

	// --- C++ access ---
	flecs::world* GetEcsWorld() const { return ECSWorld; }
	const FWindGrid& GetWindGrid() const { return WindGrid; }
	FWindGrid& GetWindGridMutable() { return WindGrid; }

protected:
	flecs::world* ECSWorld = nullptr;
	FWindGrid WindGrid;
	FVector AmbientWind = FVector(100.f, 0.f, 0.f); 

private:
	void RegisterComponents();
	void RegisterSystems();
	void DrawDebugWind();

	// Track HISM components that need MarkRenderStateDirty after ECS progress
	TSet<UHierarchicalInstancedStaticMeshComponent*> DirtyHISMs;
};
