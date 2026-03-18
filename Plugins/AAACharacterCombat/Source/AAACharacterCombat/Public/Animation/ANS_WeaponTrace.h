#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_WeaponTrace.generated.h"

/**
 * Anim Notify State that performs weapon hit detection via sphere sweep
 * between two mesh sockets over the duration of the notify window.
 * On hit, sends Event_Combat_Hit gameplay event through the owner's ASC.
 */
UCLASS(DisplayName = "Weapon Trace", meta = (ShowWorldContextPin))
class AAACHARACTERCOMBAT_API UANS_WeaponTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UANS_WeaponTrace();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** Socket at the base of the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Trace")
	FName TraceStartSocket;

	/** Socket at the tip of the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Trace")
	FName TraceEndSocket;

	/** Radius for sphere sweep */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Trace", meta = (ClampMin = "0.0"))
	float TraceRadius;

	/** Collision channel used for the sweep */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Draw debug shapes in editor / development builds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Trace|Debug")
	bool bDebugDraw;

private:
	/** Previous frame socket locations for interpolated sweep */
	FVector PreviousStartLocation;
	FVector PreviousEndLocation;

	/** Actors already hit during this notify window (prevents double-hits) */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> AlreadyHitActors;

	/** Helper: get socket world location from mesh */
	FVector GetMeshSocketLocation(USkeletalMeshComponent* MeshComp, const FName& SocketName) const;

	/** Perform the sphere sweep and process results */
	void PerformTrace(USkeletalMeshComponent* MeshComp);
};
