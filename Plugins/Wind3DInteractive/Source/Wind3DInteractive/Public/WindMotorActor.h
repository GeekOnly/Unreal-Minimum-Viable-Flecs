#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindTypes.h"
#include "WindSubsystem.h"
#include "WindMotorActor.generated.h"

class UWindVolumeComponent;
class UArrowComponent;

UCLASS(Blueprintable, BlueprintType, ClassGroup = "Wind3D",
	meta = (DisplayName = "Wind Motor Actor"))
class WIND3DINTERACTIVE_API AWindMotorActor : public AActor
{
	GENERATED_BODY()

public:
	AWindMotorActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaTime) override;

	/** Access the wind volume component (single source of truth for all wind params). */
	UFUNCTION(BlueprintCallable, Category = "Wind Motor")
	UWindVolumeComponent* GetWindVolume() const { return WindVolumeComp; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWindVolumeComponent> WindVolumeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> DirectionArrow;

private:
	FWindEntityHandle ECSHandle;

	UWindSubsystem* GetWindSubsystem() const;
};
