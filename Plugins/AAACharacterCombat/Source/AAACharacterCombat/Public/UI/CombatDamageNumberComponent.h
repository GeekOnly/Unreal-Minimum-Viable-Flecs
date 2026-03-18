#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatDamageNumberComponent.generated.h"

class UUserWidget;
class UWidgetComponent;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class AAACHARACTERCOMBAT_API UCombatDamageNumberComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatDamageNumberComponent();

	/** Widget class to use for damage number display. The BP should handle fly-up and fade-out animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Numbers")
	TSubclassOf<UUserWidget> DamageNumberWidgetClass;

	/** Speed at which the damage number floats upward (units/sec). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Numbers")
	float FloatSpeed = 100.f;

	/** How long the damage number lives before being destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Numbers")
	float FloatDuration = 1.5f;

	/** Random XY spread applied to spawn location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Damage Numbers")
	FVector2D RandomSpread = FVector2D(30.0, 30.0);

	/**
	 * Spawn a floating damage number at the given world location.
	 * @param Damage       Damage amount to display.
	 * @param WorldLocation World-space position to spawn the number.
	 * @param bIsCritical  If true, the widget can style itself differently.
	 */
	UFUNCTION(BlueprintCallable, Category = "Damage Numbers")
	void SpawnDamageNumber(float Damage, FVector WorldLocation, bool bIsCritical = false);
};
