// Round Survival

#pragma once

#include "Animation/AnimInstance.h"

#include "RSWeaponAnimInstance.generated.h"

class ARSPlayerCharacter;

/**
 * Animation instance for a weapon's own skeletal mesh: the receiver, whose bolt, magazine catch and
 * spare magazine the weapon montages move.
 *
 * Those montages carry two tracks, as the character's do: the hip clip in DefaultSlot and the aimed
 * clip in Aiming. Both play from one montage position, so they never drift apart; the graph mixes
 * them by AimDownSightsAlpha, the same value the hands are mixed by, so the spare magazine stays in
 * the hand that carries it when the player aims in or out halfway through a reload.
 */
UCLASS()
class ROUND_SURVIVAL_API URSWeaponAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	//~UAnimInstance interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	//~End of UAnimInstance interface

	/** The holder's ARSPlayerCharacter::GetAimDownSightsAlpha. Zero on a weapon nobody holds, such as one on a Wall Locker. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Weapon")
	float AimDownSightsAlpha = 0.0f;

private:
	/** The weapon actor's owner, which the equipment spawns it with. */
	TWeakObjectPtr<const ARSPlayerCharacter> Holder;
};
