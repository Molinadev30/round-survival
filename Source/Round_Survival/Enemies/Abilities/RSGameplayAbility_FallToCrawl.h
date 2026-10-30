// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_FallToCrawl.generated.h"

class UAnimMontage;

/**
 * An enemy that has lost a leg going down onto its hands, for the rest of its life.
 *
 * Triggered by the tag the leg's sever effect grants, `RS.State.Crawling`, when it is first added:
 * the second leg adds to a tag the enemy already has and starts nothing. The fall crouches the
 * body — crawling is the movement component's crouch, see ARSZombieCharacter — and plays the drop
 * to the floor. Nothing has to end the crawl: the tag lasts as long as the effect, which lasts as
 * long as the life, and the abilities that look different on the floor pick their montage from it.
 *
 * Whatever swing was under way is cancelled, since a body losing a leg mid-swing does not finish
 * it, and every enemy action is blocked until it is down. A climb through a window is the
 * exception: cut short, the warp would leave the body inside the frame, so the fall waits for
 * `RS.State.TraversingWindow` to go and happens on the far side.
 *
 * Server only and unpredicted. The montage carries root motion, so where the body comes to rest is
 * the server's; the crouch and the montage reach clients the ordinary way.
 */
UCLASS(Abstract)
class ROUND_SURVIVAL_API URSGameplayAbility_FallToCrawl : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_FallToCrawl(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End of UGameplayAbility interface

	/** Crouches the body and plays the fall. */
	void Fall();

	UFUNCTION()
	void HandleClimbEnded();

	UFUNCTION()
	void HandleMontageEnded();

	/** Standing to the floor, with root motion: the body pitches forward as it goes down. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Crawl")
	TObjectPtr<UAnimMontage> FallMontage;
};
