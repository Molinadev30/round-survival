// Round Survival

#pragma once

#include "Equipment/RSGameplayAbility_FromEquipment.h"

#include "RSGameplayAbility_AimDownSights.generated.h"

/**
 * Aiming down the sights, for as long as the input is held.
 *
 * It is a state, and all the ability does is hold it: `RS.State.ADS` for as long as it runs, which the
 * weapon's spread, the camera's zoom and the crosshair read, and the movement component's aim flag,
 * which slows the player inside the predicted move (URSCharacterMovementComponent explains why there
 * and not on an attribute). Predicted, so the zoom starts on the frame the button goes down.
 *
 * Granted by the weapon, like firing and reloading: no weapon in hand, nothing to aim. Its numbers —
 * how far it zooms, how long it takes — are the weapon's (URSRangedWeaponInstance). Reloading keeps it,
 * by design; switching weapons, going down and a hold that takes both hands end it, each
 * through its own list of abilities to cancel, the way they already end a burst or a reload.
 */
UCLASS()
class ROUND_SURVIVAL_API URSGameplayAbility_AimDownSights : public URSGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:
	URSGameplayAbility_AimDownSights(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UGameplayAbility interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

private:
	/** Raises or lowers the movement component's aim flag. Only on the machine that controls the pawn. */
	static void SetMovementAimFlag(const FGameplayAbilityActorInfo* ActorInfo, bool bAiming);
};
