#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatComboWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UWidgetAnimation;

UCLASS()
class AAACHARACTERCOMBAT_API UCombatComboWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Text displaying the current combo count. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ComboCountText;

	/** Label text (e.g. "COMBO" or "HIT"). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ComboLabelText;

	/** Boost / special gauge progress bar. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> BoostGauge;

	/** Health bar. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	/** Stamina bar. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaBar;

	/** Optional pop animation played when combo count changes. */
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> ComboPopAnim;

	/** Update the combo counter display. Hides combo section when Count == 0. */
	UFUNCTION(BlueprintCallable, Category = "Combat UI|Combo")
	void UpdateComboCount(int32 Count);

	/** Update the boost gauge. */
	UFUNCTION(BlueprintCallable, Category = "Combat UI|Gauges")
	void UpdateBoost(float Current, float Max);

	/** Update the health bar. */
	UFUNCTION(BlueprintCallable, Category = "Combat UI|Gauges")
	void UpdateHealth(float Current, float Max);

	/** Update the stamina bar. */
	UFUNCTION(BlueprintCallable, Category = "Combat UI|Gauges")
	void UpdateStamina(float Current, float Max);

protected:
	virtual void NativeConstruct() override;
};
