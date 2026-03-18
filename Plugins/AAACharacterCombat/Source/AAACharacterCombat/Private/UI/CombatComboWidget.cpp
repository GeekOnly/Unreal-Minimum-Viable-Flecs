#include "UI/CombatComboWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Animation/WidgetAnimation.h"

void UCombatComboWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize all bars to full
	if (BoostGauge)
	{
		BoostGauge->SetPercent(1.f);
	}
	if (HealthBar)
	{
		HealthBar->SetPercent(1.f);
	}
	if (StaminaBar)
	{
		StaminaBar->SetPercent(1.f);
	}

	// Hide combo section by default
	if (ComboCountText)
	{
		ComboCountText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ComboLabelText)
	{
		ComboLabelText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCombatComboWidget::UpdateComboCount(int32 Count)
{
	if (!ComboCountText || !ComboLabelText)
	{
		return;
	}

	if (Count > 0)
	{
		ComboCountText->SetText(FText::AsNumber(Count));
		ComboCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		ComboLabelText->SetVisibility(ESlateVisibility::HitTestInvisible);

		// Play pop animation if available
		if (ComboPopAnim)
		{
			PlayAnimation(ComboPopAnim, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
		}
	}
	else
	{
		ComboCountText->SetVisibility(ESlateVisibility::Collapsed);
		ComboLabelText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCombatComboWidget::UpdateBoost(float Current, float Max)
{
	if (BoostGauge && Max > 0.f)
	{
		BoostGauge->SetPercent(FMath::Clamp(Current / Max, 0.f, 1.f));
	}
}

void UCombatComboWidget::UpdateHealth(float Current, float Max)
{
	if (HealthBar && Max > 0.f)
	{
		HealthBar->SetPercent(FMath::Clamp(Current / Max, 0.f, 1.f));
	}
}

void UCombatComboWidget::UpdateStamina(float Current, float Max)
{
	if (StaminaBar && Max > 0.f)
	{
		StaminaBar->SetPercent(FMath::Clamp(Current / Max, 0.f, 1.f));
	}
}
