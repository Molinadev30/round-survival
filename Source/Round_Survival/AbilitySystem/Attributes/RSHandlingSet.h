// Round Survival

#pragma once

#include "AbilitySystem/Attributes/RSHealthSet.h"

#include "RSHandlingSet.generated.h"

/**
 * How fast the owner works a weapon, and how long they can sprint. Player only.
 *
 * Split out of URSCombatSet rather than sharing it, because enemies never reload, aim down
 * sights, swap or sprint: every attribute here would be dead weight replicated on all 64 of them.
 * It is also the set the buff stations grow into — Rapid Hands modifies ReloadSpeedMultiplier,
 * Overcharge FireRateMultiplier, Second Wind ReviveSpeedMultiplier — and the ADS and swap speeds that a later station or weapon perk
 * needs belong beside them.
 *
 * These scale what the owning client runs itself — its own montages, its own clocks, its own
 * predicted moves — and the server keeps its own copy of each to check it against; nobody else
 * needs them. Hence COND_OwnerOnly, as with Scrap.
 *
 * Every value is a multiplier where 1.0 is the authored value. The weapon's own timings stay in
 * its data asset and the sprint's in the movement component — this only scales them.
 */
UCLASS()
class ROUND_SURVIVAL_API URSHandlingSet : public URSAttributeSet
{
	GENERATED_BODY()

public:
	URSHandlingSet();

	ATTRIBUTE_ACCESSORS(URSHandlingSet, ReloadSpeedMultiplier);
	ATTRIBUTE_ACCESSORS(URSHandlingSet, FireRateMultiplier);
	ATTRIBUTE_ACCESSORS(URSHandlingSet, ReviveSpeedMultiplier);
	ATTRIBUTE_ACCESSORS(URSHandlingSet, SprintDurationMultiplier);

protected:
	UFUNCTION()
	void OnRep_ReloadSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_FireRateMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ReviveSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_SprintDurationMultiplier(const FGameplayAttributeData& OldValue);

	//~UAttributeSet interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	//~End of UAttributeSet interface

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	/** Scales the reload montage play rate. Raised by the Rapid Hands station. */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Handling", ReplicatedUsing = OnRep_ReloadSpeedMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ReloadSpeedMultiplier;

	/**
	 * Scales the drawn weapon's cadence: the time between shots divides by it. Raised by the
	 * Overcharge station. The shooter needs it to time its own shots and the server to budget them,
	 * which is both ends of COND_OwnerOnly.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Handling", ReplicatedUsing = OnRep_FireRateMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData FireRateMultiplier;

	/**
	 * Scales how fast this player revives a teammate: the downed player's revive hold divides by it.
	 * Raised by the Second Wind station. The reviver's client runs its own hold and the server its
	 * copy, which is again both ends of COND_OwnerOnly.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Handling", ReplicatedUsing = OnRep_ReviveSpeedMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ReviveSpeedMultiplier;

	/**
	 * Scales how long a full sprint lasts before the player runs out. For a station to raise. The
	 * sprint is the movement component's and predicted: the owning client spends it in its own moves
	 * and the server replays them against its copy, so both need the multiplier.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "RS|Handling", ReplicatedUsing = OnRep_SprintDurationMultiplier, meta = (AllowPrivateAccess = true))
	FGameplayAttributeData SprintDurationMultiplier;
};
