#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AN_CombatEvent.generated.h"

/**
 * Generic anim notify that sends a gameplay event tag through the owner's ASC.
 * Useful for triggering GAS-side logic at specific animation frames.
 */
UCLASS(DisplayName = "Combat Event", meta = (ShowWorldContextPin))
class AAACHARACTERCOMBAT_API UAN_CombatEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAN_CombatEvent();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** Gameplay event tag to send */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Event")
	FGameplayTag EventTag;

	/** If true, event is sent to the current target rather than the owner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Event")
	bool bSendToTarget;
};
