// Round Survival

#pragma once

#include "AbilitySystemComponent.h"

#include "RSAbilitySystemComponent.generated.h"

/**
 * Round Survival's ability system component.
 *
 * Players host theirs on the player state with Mixed replication, so the owning client gets
 * full effect data for prediction while everyone else gets tags and cues only. Enemies host
 * theirs on the pawn with Minimal replication and never predict anything — with 64 concurrent
 * enemies, per-effect replication to clients is the first thing that would drown the channel.
 *
 * It also owns ability input. Input arrives as a gameplay tag, never as an ability class or an
 * index: the tag rides on the granted spec as a dynamic source tag, so binding a key to a new
 * ability is a data change. Presses are buffered and resolved once per frame in
 * ProcessAbilityInput rather than acted on immediately, which is what lets the activation
 * policy decide what a press means without the caller knowing.
 */
UCLASS()
class ROUND_SURVIVAL_API URSAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URSAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UAbilitySystemComponent interface
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	//~End of UAbilitySystemComponent interface

	/**
	 * Activates every granted ability marked OnSpawn that is not already running.
	 *
	 * Called once the avatar exists, because passive abilities granted before the pawn is
	 * assigned have nothing to act on yet.
	 */
	void TryActivateAbilitiesOnSpawn();

	/** Records a press. Nothing activates yet; ProcessAbilityInput decides what it means. */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** Records a release, and stops the input counting as held. */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/**
	 * Resolves a frame's worth of buffered input. Called from ARSPlayerController::PostProcessInput.
	 *
	 * Buffering matters because a press and a release can land in the same frame — a fast tap on
	 * an automatic weapon. Acting on the press immediately would start an ability that the
	 * release, processed first because it arrived on a different binding, had already cancelled.
	 */
	void ProcessAbilityInput(float DeltaSeconds, bool bGamePaused);

	/** Drops all buffered and held input. Called when the pawn is unpossessed or dies. */
	void ClearAbilityInput();

protected:
	//~UAbilitySystemComponent interface
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	//~End of UAbilitySystemComponent interface

private:
	/** Pressed and released this frame; cleared at the end of ProcessAbilityInput. */
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	/** Currently held. Survives across frames, which is what drives WhileInputActive. */
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
