// Round Survival

#pragma once

#include "AbilitySystem/Abilities/RSGameplayAbility.h"

#include "RSGameplayAbility_SwitchWeapon.generated.h"

class URSWeaponSlotsComponent;

/**
 * Holsters the drawn weapon and draws another carried one.
 *
 * An ability for the same reasons reloading is: it takes time, it has to stop other things while it
 * runs — `RS.State.SwitchingWeapon` blocks firing, reloading and aiming — and it has to feel
 * immediate on the shooter's machine without letting that machine decide what it owns.
 *
 *     holster montage of the drawn weapon   (predicted, played on each machine that runs this)
 *     server: URSWeaponSlotsComponent::SetActiveSlot  — unequip, equip the new item
 *     the new weapon draws itself in OnEquipped, on every machine that equips it
 *     wait out the draw, release the tag
 *
 * **One class, granted once per input.** The ability set grants it under `RS.InputTag.WeaponSlot.1`,
 * `.2`, `.Next` and `.Previous`, and each grant reads which tag it was given under to know where to
 * go. Four input bindings, one piece of code, and a third slot later is a new tag and a new set
 * entry rather than a new ability.
 *
 * A pawn ability, not an equipment one: it has to outlive the weapon it holsters.
 */
UCLASS()
class ROUND_SURVIVAL_API URSGameplayAbility_SwitchWeapon : public URSGameplayAbility
{
	GENERATED_BODY()

public:
	URSGameplayAbility_SwitchWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End of UGameplayAbility interface

protected:
	/** The holster is over: swap on the server, then wait out the draw. */
	UFUNCTION()
	void OnHolstered();

	UFUNCTION()
	void OnDrawn();

private:
	static URSWeaponSlotsComponent* FindWeaponSlots(const FGameplayAbilityActorInfo* ActorInfo);

	/**
	 * The slot this grant switches to, from the input tag it was granted under, or INDEX_NONE when
	 * there is nowhere to go — an empty slot, the slot already drawn, no other weapon to cycle to.
	 */
	static int32 ResolveTargetSlot(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo);

	/** Seconds the draw of the weapon in a slot lasts, read off its weapon instance class. */
	static float GetDrawDuration(const URSWeaponSlotsComponent* WeaponSlots, int32 SlotIndex);

	/** Resolved at activation, so the holster and the swap agree on where they are going. */
	int32 TargetSlot = INDEX_NONE;
};
