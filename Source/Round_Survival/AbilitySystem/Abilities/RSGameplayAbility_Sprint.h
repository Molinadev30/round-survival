// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_Sprint.generated.h"

/**
 * Sprinting, for as long as the input is held.
 *
 * Like aiming, it holds a state and nothing else: `RS.State.Sprinting` for as long as it runs, and the
 * movement component's sprint flag, which speeds the player up inside the predicted move. Whether a
 * given move really is a sprint — on the ground, standing, heading forward — is the movement
 * component's call (URSCharacterMovementComponent::IsSprinting), so the key can stay down while the
 * player strafes or stops without the ability having to notice.
 *
 * Granted with the player, not the weapon. Everything that needs the hands ends it — a shot, aiming, a
 * reload, a switch, a two-handed hold — through each one's own list of abilities to cancel, and the
 * states they hold keep it from starting again until they are over. The input is held, so once they
 * are, a key still down sprints again. Going down ends it too. Starting a sprint stands a crouching
 * player up, unless the crouch is a slide's.
 *
 * A slide does not end it. The slide is the movement component's, and while it lasts the pawn is not
 * sprinting (it is crouched); a sprint still wanted when the slide runs out is what stands the player
 * back up into one.
 */
UCLASS()
class ROUND_SURVIVAL_API URSGameplayAbility_Sprint : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UGameplayAbility interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

private:
	/** Raises or lowers the movement component's sprint flag. Only on the machine that controls the pawn. */
	static void SetMovementSprintFlag(const FGameplayAbilityActorInfo* ActorInfo, bool bSprinting);
};
