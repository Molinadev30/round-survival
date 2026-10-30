// Round Survival

#pragma once

#include "Internationalization/Text.h"
#include "UObject/Interface.h"

#include "RSInteractable.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class URSInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Something the player can look at and use: a barricade to repair, a wall locker to buy from,
 * a sector gate to open.
 *
 * Five questions, and the interactable answers all of them. Nothing on the player side knows
 * what any particular interactable is — the interactor component finds one, the interaction
 * ability asks it how long the hold is and whether it is allowed, and calls `Interact` on the
 * server when the hold completes. Adding a new kind of interactable therefore touches no
 * player code at all.
 *
 * `CanInteract` is deliberately *not* what makes something focusable. A wall locker the player
 * cannot afford still shows its prompt — that is how the player learns what it costs — so the
 * focus trace only asks for this interface, and `CanInteract` gates the activation.
 *
 * Every implementation must treat `Interact` as server-only: it is called behind an authority
 * check and a client reaching it would be a bug.
 */
class ROUND_SURVIVAL_API IRSInteractable
{
	GENERATED_BODY()

public:
	/**
	 * Stands in a prompt where the interact key goes. The HUD replaces it with whatever key is
	 * bound, so no prompt ever names a key itself.
	 */
	static const FString PromptKeyToken;

	/**
	 * The sentence every usable interactable says: "Press {Key} for Carbine [Cost: 1,000]", or
	 * "Hold {Key} to rebuild Barricade" when the interaction is a hold.
	 *
	 * @param Action       What follows the key: "for Carbine [Cost: 1,000]", "to rebuild Barricade".
	 * @param HoldSeconds  This interactable's GetHoldSeconds; above zero says Hold, zero says Press.
	 */
	static FText MakeActionPrompt(const FText& Action, float HoldSeconds);

	/**
	 * What the HUD shows while this is focused; empty shows nothing. Free to say why it cannot be
	 * used. Where it names the key it carries PromptKeyToken — build it with MakeActionPrompt.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	FText GetPrompt(AActor* Interactor) const;
	virtual FText GetPrompt_Implementation(AActor* Interactor) const { return FText::GetEmpty(); }

	/** Seconds the input must be held for one interaction. Zero interacts on the press. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	float GetHoldSeconds(AActor* Interactor) const;
	virtual float GetHoldSeconds_Implementation(AActor* Interactor) const { return 0.0f; }

	/** Whether this interactor may use it right now: affordable, not already full, powered. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const { return true; }

	/**
	 * What this interaction costs in scrap, for this interactor, right now.
	 *
	 * The price lives here rather than on the ability because it is a property of the locker, not
	 * of the key: one ability, one input tag, and every purchasable naming its own number. Zero
	 * means free, which is what a barricade returns.
	 *
	 * It is asked again at the moment the hold completes, so a wall locker can answer the purchase
	 * price the first time and the ammo refill price afterwards without anything else changing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	int32 GetScrapCost(AActor* Interactor) const;
	virtual int32 GetScrapCost_Implementation(AActor* Interactor) const { return 0; }

	/**
	 * Whether one held press may run more than one interaction.
	 *
	 * True for a barricade: a hold restores board after board, and each one pays on the frame it
	 * lands. False for a purchase, where holding the key must not buy twice.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	bool IsRepeatable() const;
	virtual bool IsRepeatable_Implementation() const { return false; }

	/**
	 * Whether using this takes the player's weapon hands.
	 *
	 * False for most things, by design: a player can shoot and reload while repairing a
	 * barricade, opening a gate or topping up ammo at a locker. True for a revive and for the
	 * machines — the stations and the Weapon Forge. While such a hold runs the player carries
	 * `RS.State.HandsBusy`, which the fire and reload abilities refuse to start under, and a burst or
	 * reload under way is cancelled as it starts. Buying a weapon needs no answer here: drawing the
	 * new weapon puts the old one away, and its reload goes with it.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	bool InterruptsWeapon(AActor* Interactor) const;
	virtual bool InterruptsWeapon_Implementation(AActor* Interactor) const { return false; }

	/**
	 * A hold on this has started. Server only. Most interactables ignore it; a downed player uses
	 * it to stop bleeding out while someone is reviving them.
	 *
	 * Every start is matched by exactly one OnInteractionHoldEnded, whether the hold completed, was
	 * let go, or was cut short, and the end comes before Interact when it completes.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	void OnInteractionHoldStarted(AActor* Interactor);
	virtual void OnInteractionHoldStarted_Implementation(AActor* Interactor) {}

	/** The hold OnInteractionHoldStarted announced is over, however it ended. Server only. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	void OnInteractionHoldEnded(AActor* Interactor);
	virtual void OnInteractionHoldEnded_Implementation(AActor* Interactor) {}

	/** Do the thing. Server only, once per completed hold. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RS|Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor) {}
};
