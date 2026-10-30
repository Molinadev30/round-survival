// Round Survival

#pragma once

#include "Equipment/RSGameplayAbility_FromEquipment.h"

#include "RSGameplayAbility_Reload.generated.h"

class URSWeaponInstance;
class URSWeaponItemInstance;

/**
 * Moving rounds from the reserve into the magazine.
 *
 * An ability rather than a function on the weapon, for three reasons that all matter later: it
 * owns `RS.State.Reloading` for as long as it runs, so animation and other abilities can read one
 * state instead of asking the weapon; it can be cancelled, which is what firing mid-reload does;
 * and its duration scales off the `ReloadSpeedMultiplier` attribute, so a buff station changes
 * reload speed without touching weapon code.
 *
 * The transfer itself only ever happens on the server. The client runs the ability for the timing
 * and the animation and receives the new counts by replication.
 */
UCLASS()
class ROUND_SURVIVAL_API URSGameplayAbility_Reload : public URSGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:
	URSGameplayAbility_Reload(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "RS|Weapon")
	URSWeaponInstance* GetWeaponInstance() const;

	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End of UGameplayAbility interface

protected:
	/** Base seconds to reload, before the reload speed attribute scales it. */
	UPROPERTY(EditDefaultsOnly, Category = "RS|Weapon", meta = (ForceUnits = s, ClampMin = 0.05))
	float ReloadDuration = 2.0f;

	/** The weapon's transfer point: move the rounds, then wait out the rest of the animation. */
	UFUNCTION()
	void OnAmmoTransferPoint();

	/** The animation is over. */
	UFUNCTION()
	void OnReloadFinished();

	/** Moves rounds from the reserve into the magazine. Server only; a client reaching here does nothing. */
	void TransferAmmo();

	/** Seconds this reload will take for this player, after the handling attribute. */
	float GetScaledReloadDuration() const;

private:
	/** How many rounds fit in this weapon's magazine, as a tag stack on its item. */
	static int32 GetMagazineSize(const URSWeaponItemInstance* Item);

	/** Seconds left between the transfer point and the end of this activation. */
	float DurationAfterTransfer = 0.0f;
};
