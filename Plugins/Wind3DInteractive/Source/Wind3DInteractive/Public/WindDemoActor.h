#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindDemoActor.generated.h"

class AWindMotorActor;
class UWindVolumeComponent;
class USceneComponent;
class UBillboardComponent;

UENUM(BlueprintType)
enum class EWindDemoMode : uint8
{
	FanTurbine   UMETA(DisplayName = "1 - Fan Turbine (Wave)"),
	Tornado      UMETA(DisplayName = "2 - Tornado (Vortex)"),
	Explosion    UMETA(DisplayName = "3 - Explosion (Omni Pulse)"),
	DirectGust   UMETA(DisplayName = "4 - Directional Gust"),
	MultiFan     UMETA(DisplayName = "5 - Multi-Fan Array"),
};

/**
 * Drop-and-play wind demo actor.
 * Place one in the level — press 1-5 to switch demo modes.
 * Each mode spawns and configures AWindMotorActor instances automatically.
 */
UCLASS(Blueprintable, ClassGroup = "Wind3D", meta = (DisplayName = "Wind Demo Actor"))
class WIND3DINTERACTIVE_API AWindDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AWindDemoActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaTime) override;

	/** Switch to a specific demo mode (also bound to keys 1-5 in BeginPlay). */
	UFUNCTION(BlueprintCallable, Category = "Wind Demo")
	void SetDemoMode(EWindDemoMode NewMode);

	/** Cycle to next mode (wrap around). */
	UFUNCTION(BlueprintCallable, Category = "Wind Demo")
	void NextMode();

	/** Current active demo mode. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Wind Demo")
	EWindDemoMode CurrentMode = EWindDemoMode::FanTurbine;

	/** Start in this mode on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Demo")
	EWindDemoMode StartMode = EWindDemoMode::FanTurbine;

	/** Radius of the motor(s) spawned by this demo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Demo", meta = (ClampMin = "100.0"))
	float MotorRadius = 400.f;

	/** Base strength applied to all motors in the demo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Demo", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
	float MotorStrength = 700.f;

	/** Spacing between motors in MultiFan mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Demo", meta = (ClampMin = "100.0"))
	float FanSpacing = 600.f;

	/** Number of fans in MultiFan mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Demo", meta = (ClampMin = "1", ClampMax = "8"))
	int32 FanCount = 3;

	/** Rotate the fan motor(s) during FanTurbine and MultiFan modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Demo")
	bool bAnimateFan = true;

	/** Fan blade rotation speed (degrees/second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind Demo", meta = (ClampMin = "0.0", ClampMax = "720.0", EditCondition = "bAnimateFan"))
	float FanRPM = 120.f;

private:
	void SpawnMotorsForMode(EWindDemoMode Mode);
	void DestroyAllMotors();

	AWindMotorActor* SpawnMotor(FVector RelativeOffset, FRotator RelativeRotation);

	// Fan animation: accumulated angle per motor
	TArray<float> FanAngles;

	UPROPERTY()
	TArray<TObjectPtr<AWindMotorActor>> ActiveMotors;
};
